/*
 * ShaderConductor
 *
 * Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License.
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
 * to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <ShaderConductor/ShaderConductor.hpp>

#include <dxc/Support/Global.h>
#include <dxc/Support/Unicode.h>
#include <dxc/Support/WinAdapter.h>
#include <dxc/Support/WinIncludes.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cctype>
#include <fstream>
#include <memory>

#include <dxc/DxilContainer/DxilContainer.h>
#include <dxc/dxcapi.h>
#include <llvm/Support/ErrorHandling.h>

#include <spirv-tools/libspirv.h>
#include <spirv.hpp>
#include <spirv_cross.hpp>
#include <spirv_cross_util.hpp>
#include <spirv_glsl.hpp>
#include <spirv_hlsl.hpp>
#include <spirv_msl.hpp>

#ifdef LLVM_ON_WIN32
#include <d3d12shader.h>
#endif

#define SC_UNUSED(x) (void)(x);

using namespace ShaderConductor;

namespace
{
    bool dllDetaching = false;

    class Dxcompiler
    {
    public:
        ~Dxcompiler()
        {
            this->Destroy();
        }

        static Dxcompiler& Instance()
        {
            static Dxcompiler instance;
            return instance;
        }

        IDxcLibrary* Library() const
        {
            return m_library;
        }

        IDxcCompiler* Compiler() const
        {
            return m_compiler;
        }

        IDxcContainerReflection* ContainerReflection() const
        {
            return m_containerReflection;
        }

        CComPtr<IDxcLinker> CreateLinker() const
        {
            CComPtr<IDxcLinker> linker;
            IFT(m_createInstanceFunc(CLSID_DxcLinker, __uuidof(IDxcLinker), reinterpret_cast<void**>(&linker)));
            return linker;
        }

        bool LinkerSupport() const
        {
            return m_linkerSupport;
        }

        void Destroy()
        {
            if (m_dxcompilerDll)
            {
                m_compiler = nullptr;
                m_library = nullptr;
                m_containerReflection = nullptr;

                m_createInstanceFunc = nullptr;

#ifdef _WIN32
                ::FreeLibrary(m_dxcompilerDll);
#else
                ::dlclose(m_dxcompilerDll);
#endif

                m_dxcompilerDll = nullptr;
            }
        }

        void Terminate()
        {
            if (m_dxcompilerDll)
            {
                m_compiler.Detach();
                m_library.Detach();
                m_containerReflection.Detach();

                m_createInstanceFunc = nullptr;

                m_dxcompilerDll = nullptr;
            }
        }

    private:
        Dxcompiler()
        {
            if (dllDetaching)
            {
                return;
            }

#ifdef _WIN32
            const char* dllName = "dxcompiler.dll";
#elif __APPLE__
            const char* dllName = "libdxcompiler.dylib";
#else
            const char* dllName = "libdxcompiler.so";
#endif
            const char* functionName = "DxcCreateInstance";

#ifdef _WIN32
            m_dxcompilerDll = ::LoadLibraryA(dllName);
#else
            m_dxcompilerDll = ::dlopen(dllName, RTLD_LAZY);
#endif

            if (m_dxcompilerDll != nullptr)
            {
#ifdef _WIN32
                m_createInstanceFunc = (DxcCreateInstanceProc)::GetProcAddress(m_dxcompilerDll, functionName);
#else
                m_createInstanceFunc = (DxcCreateInstanceProc)::dlsym(m_dxcompilerDll, functionName);
#endif

                if (m_createInstanceFunc != nullptr)
                {
                    IFT(m_createInstanceFunc(CLSID_DxcLibrary, __uuidof(IDxcLibrary), reinterpret_cast<void**>(&m_library)));
                    IFT(m_createInstanceFunc(CLSID_DxcCompiler, __uuidof(IDxcCompiler), reinterpret_cast<void**>(&m_compiler)));
                    IFT(m_createInstanceFunc(CLSID_DxcContainerReflection, __uuidof(IDxcContainerReflection),
                                             reinterpret_cast<void**>(&m_containerReflection)));
                }
                else
                {
                    this->Destroy();

                    throw std::runtime_error(std::string("COULDN'T get ") + functionName + " from dxcompiler.");
                }
            }
            else
            {
                throw std::runtime_error("COULDN'T load dxcompiler.");
            }

            m_linkerSupport = (CreateLinker() != nullptr);
        }

    private:
        HMODULE m_dxcompilerDll = nullptr;
        DxcCreateInstanceProc m_createInstanceFunc = nullptr;

        CComPtr<IDxcLibrary> m_library;
        CComPtr<IDxcCompiler> m_compiler;
        CComPtr<IDxcContainerReflection> m_containerReflection;

        bool m_linkerSupport;
    };

    class ScIncludeHandler : public IDxcIncludeHandler
    {
    public:
        explicit ScIncludeHandler(std::function<Blob(const char* includeName)> loadCallback) : m_loadCallback(std::move(loadCallback))
        {
        }

        HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR fileName, IDxcBlob** includeSource) override
        {
            if ((fileName[0] == L'.') && (fileName[1] == L'/'))
            {
                fileName += 2;
            }

            std::string utf8FileName;
            if (!Unicode::UTF16ToUTF8String(fileName, &utf8FileName))
            {
                return E_FAIL;
            }

            Blob source;
            try
            {
                source = m_loadCallback(utf8FileName.c_str());
            }
            catch (...)
            {
                return E_FAIL;
            }

            *includeSource = nullptr;
            return Dxcompiler::Instance().Library()->CreateBlobWithEncodingOnHeapCopy(source.Data(), source.Size(), CP_UTF8,
                                                                                      reinterpret_cast<IDxcBlobEncoding**>(includeSource));
        }

        ULONG STDMETHODCALLTYPE AddRef() override
        {
            ++m_ref;
            return m_ref;
        }

        ULONG STDMETHODCALLTYPE Release() override
        {
            --m_ref;
            ULONG result = m_ref;
            if (result == 0)
            {
                delete this;
            }
            return result;
        }

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
        {
            if (IsEqualIID(iid, __uuidof(IDxcIncludeHandler)))
            {
                *object = dynamic_cast<IDxcIncludeHandler*>(this);
                this->AddRef();
                return S_OK;
            }
            else if (IsEqualIID(iid, __uuidof(IUnknown)))
            {
                *object = dynamic_cast<IUnknown*>(this);
                this->AddRef();
                return S_OK;
            }
            else
            {
                return E_NOINTERFACE;
            }
        }

    private:
        std::function<Blob(const char* includeName)> m_loadCallback;

        std::atomic<ULONG> m_ref = 0;
    };

    Blob DefaultLoadCallback(const char* includeName)
    {
        std::vector<char> ret;
        std::ifstream includeFile(includeName, std::ios_base::in);
        if (includeFile)
        {
            includeFile.seekg(0, std::ios::end);
            ret.resize(static_cast<size_t>(includeFile.tellg()));
            includeFile.seekg(0, std::ios::beg);
            includeFile.read(ret.data(), ret.size());
            ret.resize(static_cast<size_t>(includeFile.gcount()));
        }
        else
        {
            throw std::runtime_error(std::string("COULDN'T load included file ") + includeName + ".");
        }
        return Blob(ret.data(), static_cast<uint32_t>(ret.size()));
    }

    void AppendError(Compiler::ResultDesc& result, const std::string& msg)
    {
        std::string errorMSg;
        if (result.errorWarningMsg.Size() != 0)
        {
            errorMSg.assign(reinterpret_cast<const char*>(result.errorWarningMsg.Data()), result.errorWarningMsg.Size());
        }
        if (!errorMSg.empty())
        {
            errorMSg += "\n";
        }
        errorMSg += msg;
        result.errorWarningMsg.Reset(errorMSg.data(), static_cast<uint32_t>(errorMSg.size()));
        result.hasError = true;
    }

#ifdef LLVM_ON_WIN32
    template <typename T>
    HRESULT CreateDxcReflectionFromBlob(IDxcBlob* dxilBlob, CComPtr<T>& outReflection)
    {
        IDxcContainerReflection* containReflection = Dxcompiler::Instance().ContainerReflection();
        IFT(containReflection->Load(dxilBlob));

        uint32_t dxilPartIndex = ~0u;
        IFT(containReflection->FindFirstPartKind(hlsl::DFCC_DXIL, &dxilPartIndex));
        HRESULT result = containReflection->GetPartReflection(dxilPartIndex, __uuidof(T), reinterpret_cast<void**>(&outReflection));

        return result;
    }
#endif

    std::wstring ShaderProfileName(ShaderStage stage, Compiler::ShaderModel shaderModel)
    {
        std::wstring shaderProfile;
        switch (stage)
        {
        case ShaderStage::VertexShader:
            shaderProfile = L"vs";
            break;

        case ShaderStage::PixelShader:
            shaderProfile = L"ps";
            break;

        case ShaderStage::GeometryShader:
            shaderProfile = L"gs";
            break;

        case ShaderStage::HullShader:
            shaderProfile = L"hs";
            break;

        case ShaderStage::DomainShader:
            shaderProfile = L"ds";
            break;

        case ShaderStage::ComputeShader:
            shaderProfile = L"cs";
            break;

        default:
            llvm_unreachable("Invalid shader stage.");
        }

        shaderProfile.push_back(L'_');
        shaderProfile.push_back(L'0' + shaderModel.major_ver);
        shaderProfile.push_back(L'_');
        shaderProfile.push_back(L'0' + shaderModel.minor_ver);

        return shaderProfile;
    }

#ifdef LLVM_ON_WIN32
    Reflection MakeDxilReflection(IDxcBlob* dxilBlob);
#endif
    Reflection MakeSpirVReflection(const spirv_cross::Compiler& compiler);

    void ConvertDxcResult(Compiler::ResultDesc& result, IDxcOperationResult* dxcResult, ShadingLanguage targetLanguage, bool asModule,
                          bool needReflection)
    {
        HRESULT status;
        IFT(dxcResult->GetStatus(&status));

        result.target.Reset();
        result.errorWarningMsg.Reset();

        CComPtr<IDxcBlobEncoding> errors;
        IFT(dxcResult->GetErrorBuffer(&errors));
        if (errors != nullptr)
        {
            result.errorWarningMsg.Reset(errors->GetBufferPointer(), static_cast<uint32_t>(errors->GetBufferSize()));
            errors = nullptr;
        }

        result.hasError = true;
        if (SUCCEEDED(status))
        {
            CComPtr<IDxcBlob> program;
            IFT(dxcResult->GetResult(&program));
            dxcResult = nullptr;
            if (program != nullptr)
            {
                result.target.Reset(program->GetBufferPointer(), static_cast<uint32_t>(program->GetBufferSize()));
                result.hasError = false;
            }

            if (needReflection)
            {
#ifdef LLVM_ON_WIN32
                if ((targetLanguage == ShadingLanguage::Dxil) && !asModule)
                {
                    // Gather reflection information only for ShadingLanguage::Dxil. SPIR-V reflection is gathered when cross-compiling.
                    result.reflection = MakeDxilReflection(program);
                }
#else
                SC_UNUSED(targetLanguage);
                SC_UNUSED(asModule);
#endif
            }
        }
    }

    Compiler::ResultDesc CompileToBinary(const Compiler::SourceDesc& source, const Compiler::Options& options,
                                         ShadingLanguage targetLanguage, bool asModule)
    {
        assert((targetLanguage == ShadingLanguage::Dxil) || (targetLanguage == ShadingLanguage::SpirV));

        std::wstring shaderProfile;
        if (asModule)
        {
            if (targetLanguage == ShadingLanguage::Dxil)
            {
                shaderProfile = L"lib_6_x";
            }
            else
            {
                llvm_unreachable("Spir-V module is not supported.");
            }
        }
        else
        {
            shaderProfile = ShaderProfileName(source.stage, options.shaderModel);
        }

        std::vector<DxcDefine> dxcDefines;
        std::vector<std::wstring> dxcDefineStrings;
        // Need to reserve capacity so that small-string optimization does not
        // invalidate the pointers to internal string data while resizing.
        dxcDefineStrings.reserve(source.numDefines * 2);
        for (size_t i = 0; i < source.numDefines; ++i)
        {
            const auto& define = source.defines[i];

            std::wstring nameUtf16Str;
            Unicode::UTF8ToUTF16String(define.name, &nameUtf16Str);
            dxcDefineStrings.emplace_back(std::move(nameUtf16Str));
            const wchar_t* nameUtf16 = dxcDefineStrings.back().c_str();

            const wchar_t* valueUtf16;
            if (define.value != nullptr)
            {
                std::wstring valueUtf16Str;
                Unicode::UTF8ToUTF16String(define.value, &valueUtf16Str);
                dxcDefineStrings.emplace_back(std::move(valueUtf16Str));
                valueUtf16 = dxcDefineStrings.back().c_str();
            }
            else
            {
                valueUtf16 = nullptr;
            }

            dxcDefines.push_back({nameUtf16, valueUtf16});
        }

        CComPtr<IDxcBlobEncoding> sourceBlob;
        IFT(Dxcompiler::Instance().Library()->CreateBlobWithEncodingOnHeapCopy(
            source.source, static_cast<UINT32>(std::strlen(source.source)), CP_UTF8, &sourceBlob));
        IFTARG(sourceBlob->GetBufferSize() >= 4);

        std::wstring shaderNameUtf16;
        Unicode::UTF8ToUTF16String(source.fileName, &shaderNameUtf16);

        std::wstring entryPointUtf16;
        Unicode::UTF8ToUTF16String(source.entryPoint, &entryPointUtf16);

        std::vector<std::wstring> dxcArgStrings;

        // HLSL matrices are translated into SPIR-V OpTypeMatrixs in a transposed manner,
        // See also https://antiagainst.github.io/post/hlsl-for-vulkan-matrices/
        if (options.packMatricesInRowMajor)
        {
            dxcArgStrings.push_back(L"-Zpr");
        }
        else
        {
            dxcArgStrings.push_back(L"-Zpc");
        }

        if (options.enable16bitTypes)
        {
            if (options.shaderModel >= Compiler::ShaderModel{6, 2})
            {
                dxcArgStrings.push_back(L"-enable-16bit-types");
            }
            else
            {
                throw std::runtime_error("16-bit types requires shader model 6.2 or up.");
            }
        }

        if (options.enableDebugInfo)
        {
            dxcArgStrings.push_back(L"-Zi");
        }

        if (options.disableOptimizations)
        {
            dxcArgStrings.push_back(L"-Od");
        }
        else
        {
            if (options.optimizationLevel < 4)
            {
                dxcArgStrings.push_back(std::wstring(L"-O") + static_cast<wchar_t>(L'0' + options.optimizationLevel));
            }
            else
            {
                llvm_unreachable("Invalid optimization level.");
            }
        }

        if (options.shiftAllCBuffersBindings > 0)
        {
            dxcArgStrings.push_back(L"-fvk-b-shift");
            dxcArgStrings.push_back(std::to_wstring(options.shiftAllCBuffersBindings));
            dxcArgStrings.push_back(L"all");
        }

        if (options.shiftAllUABuffersBindings > 0)
        {
            dxcArgStrings.push_back(L"-fvk-u-shift");
            dxcArgStrings.push_back(std::to_wstring(options.shiftAllUABuffersBindings));
            dxcArgStrings.push_back(L"all");
        }

        if (options.shiftAllSamplersBindings > 0)
        {
            dxcArgStrings.push_back(L"-fvk-s-shift");
            dxcArgStrings.push_back(std::to_wstring(options.shiftAllSamplersBindings));
            dxcArgStrings.push_back(L"all");
        }

        if (options.shiftAllTexturesBindings > 0)
        {
            dxcArgStrings.push_back(L"-fvk-t-shift");
            dxcArgStrings.push_back(std::to_wstring(options.shiftAllTexturesBindings));
            dxcArgStrings.push_back(L"all");
        }

        switch (targetLanguage)
        {
        case ShadingLanguage::Dxil:
            break;

        case ShadingLanguage::SpirV:
        case ShadingLanguage::Hlsl:
        case ShadingLanguage::Glsl:
        case ShadingLanguage::Essl:
        case ShadingLanguage::Msl_macOS:
        case ShadingLanguage::Msl_iOS:
            dxcArgStrings.push_back(L"-spirv");
            break;

        default:
            llvm_unreachable("Invalid shading language.");
        }

        std::vector<const wchar_t*> dxcArgs;
        dxcArgs.reserve(dxcArgStrings.size());
        for (const auto& arg : dxcArgStrings)
        {
            dxcArgs.push_back(arg.c_str());
        }

        CComPtr<IDxcIncludeHandler> includeHandler = new ScIncludeHandler(std::move(source.loadIncludeCallback));
        CComPtr<IDxcOperationResult> compileResult;
        IFT(Dxcompiler::Instance().Compiler()->Compile(sourceBlob, shaderNameUtf16.c_str(), entryPointUtf16.c_str(), shaderProfile.c_str(),
                                                       dxcArgs.data(), static_cast<UINT32>(dxcArgs.size()), dxcDefines.data(),
                                                       static_cast<UINT32>(dxcDefines.size()), includeHandler, &compileResult));

        Compiler::ResultDesc ret{};
        ConvertDxcResult(ret, compileResult, targetLanguage, asModule, options.needReflection);

        return ret;
    }

    Compiler::ResultDesc CrossCompile(const Compiler::ResultDesc& binaryResult, const Compiler::SourceDesc& source,
                                      const Compiler::Options& options, const Compiler::TargetDesc& target)
    {
        assert((target.language != ShadingLanguage::Dxil) && (target.language != ShadingLanguage::SpirV));
        assert((binaryResult.target.Size() & (sizeof(uint32_t) - 1)) == 0);

        Compiler::ResultDesc ret;

        ret.errorWarningMsg = binaryResult.errorWarningMsg;
        ret.isText = true;

        uint32_t intVersion = 0;
        if (target.version != nullptr)
        {
            intVersion = std::stoi(target.version);
        }

        const uint32_t* spirvIr = reinterpret_cast<const uint32_t*>(binaryResult.target.Data());
        const size_t spirvSize = binaryResult.target.Size() / sizeof(uint32_t);

        std::unique_ptr<spirv_cross::CompilerGLSL> compiler;
        bool combinedImageSamplers = false;
        bool buildDummySampler = false;

        switch (target.language)
        {
        case ShadingLanguage::Hlsl:
            if ((source.stage == ShaderStage::GeometryShader) || (source.stage == ShaderStage::HullShader) ||
                (source.stage == ShaderStage::DomainShader))
            {
                // Check https://github.com/KhronosGroup/SPIRV-Cross/issues/121 for details
                AppendError(ret, "GS, HS, and DS has not been supported yet.");
                return ret;
            }
            if ((source.stage == ShaderStage::GeometryShader) && (intVersion < 40))
            {
                AppendError(ret, "HLSL shader model earlier than 4.0 doesn't have GS or CS.");
                return ret;
            }
            if ((source.stage == ShaderStage::ComputeShader) && (intVersion < 50))
            {
                AppendError(ret, "CS in HLSL shader model earlier than 5.0 is not supported.");
                return ret;
            }
            if (((source.stage == ShaderStage::HullShader) || (source.stage == ShaderStage::DomainShader)) && (intVersion < 50))
            {
                AppendError(ret, "HLSL shader model earlier than 5.0 doesn't have HS or DS.");
                return ret;
            }
            compiler = std::make_unique<spirv_cross::CompilerHLSL>(spirvIr, spirvSize);
            break;

        case ShadingLanguage::Glsl:
        case ShadingLanguage::Essl:
            compiler = std::make_unique<spirv_cross::CompilerGLSL>(spirvIr, spirvSize);
            combinedImageSamplers = true;
            buildDummySampler = true;

            // Legacy GLSL fixups
            if (intVersion <= 300)
            {
                auto vars = compiler->get_active_interface_variables();
                for (auto& var : vars)
                {
                    auto varClass = compiler->get_storage_class(var);

                    // Make VS out and PS in variable names match
                    if ((source.stage == ShaderStage::VertexShader) && (varClass == spv::StorageClass::StorageClassOutput))
                    {
                        auto name = compiler->get_name(var);
                        if ((name.find("out_var_") == 0) || (name.find("out.var.") == 0))
                        {
                            name.replace(0, 8, "varying_");
                            compiler->set_name(var, name);
                        }
                    }
                    else if ((source.stage == ShaderStage::PixelShader) && (varClass == spv::StorageClass::StorageClassInput))
                    {
                        auto name = compiler->get_name(var);
                        if ((name.find("in_var_") == 0) || (name.find("in.var.") == 0))
                        {
                            name.replace(0, 7, "varying_");
                            compiler->set_name(var, name);
                        }
                    }
                }
            }
            break;

        case ShadingLanguage::Msl_macOS:
        case ShadingLanguage::Msl_iOS:
            if (source.stage == ShaderStage::GeometryShader)
            {
                AppendError(ret, "MSL doesn't have GS.");
                return ret;
            }
            compiler = std::make_unique<spirv_cross::CompilerMSL>(spirvIr, spirvSize);
            break;

        default:
            llvm_unreachable("Invalid target language.");
        }

        spv::ExecutionModel model;
        switch (source.stage)
        {
        case ShaderStage::VertexShader:
            model = spv::ExecutionModelVertex;
            break;

        case ShaderStage::HullShader:
            model = spv::ExecutionModelTessellationControl;
            break;

        case ShaderStage::DomainShader:
            model = spv::ExecutionModelTessellationEvaluation;
            break;

        case ShaderStage::GeometryShader:
            model = spv::ExecutionModelGeometry;
            break;

        case ShaderStage::PixelShader:
            model = spv::ExecutionModelFragment;
            break;

        case ShaderStage::ComputeShader:
            model = spv::ExecutionModelGLCompute;
            break;

        default:
            llvm_unreachable("Invalid shader stage.");
        }
        compiler->set_entry_point(source.entryPoint, model);

        spirv_cross::CompilerGLSL::Options opts = compiler->get_common_options();
        if (target.version != nullptr)
        {
            opts.version = intVersion;
        }
        opts.es = (target.language == ShadingLanguage::Essl);
        opts.force_temporary = false;
        opts.separate_shader_objects = true;
        opts.flatten_multidimensional_arrays = false;
        opts.enable_420pack_extension =
            (target.language == ShadingLanguage::Glsl) && ((target.version == nullptr) || (opts.version >= 420));
        opts.vulkan_semantics = false;
        opts.vertex.fixup_clipspace = false;
        opts.vertex.flip_vert_y = false;
        opts.vertex.support_nonzero_base_instance = true;
        compiler->set_common_options(opts);

        if (target.language == ShadingLanguage::Hlsl)
        {
            auto* hlslCompiler = static_cast<spirv_cross::CompilerHLSL*>(compiler.get());
            auto hlslOpts = hlslCompiler->get_hlsl_options();
            if (target.version != nullptr)
            {
                if (opts.version < 30)
                {
                    AppendError(ret, "HLSL shader model earlier than 3.0 is not supported.");
                    return ret;
                }
                hlslOpts.shader_model = opts.version;
            }

            if (hlslOpts.shader_model <= 30)
            {
                combinedImageSamplers = true;
                buildDummySampler = true;
            }

            hlslCompiler->set_hlsl_options(hlslOpts);
        }
        else if ((target.language == ShadingLanguage::Msl_macOS) || (target.language == ShadingLanguage::Msl_iOS))
        {
            auto* mslCompiler = static_cast<spirv_cross::CompilerMSL*>(compiler.get());
            auto mslOpts = mslCompiler->get_msl_options();
            if (target.version != nullptr)
            {
                mslOpts.msl_version = opts.version;
            }
            mslOpts.swizzle_texture_samples = false;
            mslOpts.platform = (target.language == ShadingLanguage::Msl_iOS) ? spirv_cross::CompilerMSL::Options::iOS
                                                                             : spirv_cross::CompilerMSL::Options::macOS;

            mslCompiler->set_msl_options(mslOpts);

            const auto& resources = mslCompiler->get_shader_resources();

            uint32_t textureBinding = 0;
            for (const auto& image : resources.separate_images)
            {
                mslCompiler->set_decoration(image.id, spv::DecorationBinding, textureBinding);
                ++textureBinding;
            }

            uint32_t samplerBinding = 0;
            for (const auto& sampler : resources.separate_samplers)
            {
                mslCompiler->set_decoration(sampler.id, spv::DecorationBinding, samplerBinding);
                ++samplerBinding;
            }
        }

        if (buildDummySampler)
        {
            const uint32_t sampler = compiler->build_dummy_sampler_for_combined_images();
            if (sampler != 0)
            {
                compiler->set_decoration(sampler, spv::DecorationDescriptorSet, 0);
                compiler->set_decoration(sampler, spv::DecorationBinding, 0);
            }
        }

        if (combinedImageSamplers)
        {
            compiler->build_combined_image_samplers();

            if (options.inheritCombinedSamplerBindings)
            {
                spirv_cross_util::inherit_combined_sampler_bindings(*compiler);
            }

            for (auto& remap : compiler->get_combined_image_samplers())
            {
                compiler->set_name(remap.combined_id,
                                   "SPIRV_Cross_Combined" + compiler->get_name(remap.image_id) + compiler->get_name(remap.sampler_id));
            }
        }

        if (target.language == ShadingLanguage::Hlsl)
        {
            auto* hlslCompiler = static_cast<spirv_cross::CompilerHLSL*>(compiler.get());
            const uint32_t newBuiltin = hlslCompiler->remap_num_workgroups_builtin();
            if (newBuiltin)
            {
                compiler->set_decoration(newBuiltin, spv::DecorationDescriptorSet, 0);
                compiler->set_decoration(newBuiltin, spv::DecorationBinding, 0);
            }
        }

        try
        {
            const std::string targetStr = compiler->compile();
            ret.target.Reset(targetStr.data(), static_cast<uint32_t>(targetStr.size()));
            ret.hasError = false;
            if (options.needReflection)
            {
                ret.reflection = MakeSpirVReflection(*compiler);
            }
        }
        catch (spirv_cross::CompilerError& error)
        {
            const char* errorMsg = error.what();
            ret.errorWarningMsg.Reset(errorMsg, static_cast<uint32_t>(std::strlen(errorMsg)));
            ret.hasError = true;
        }

        return ret;
    }

    Compiler::ResultDesc ConvertBinary(const Compiler::ResultDesc& binaryResult, const Compiler::SourceDesc& source,
                                       const Compiler::Options& options, const Compiler::TargetDesc& target)
    {
        if (!binaryResult.hasError)
        {
            if (target.asModule)
            {
                return binaryResult;
            }
            else
            {
                switch (target.language)
                {
                case ShadingLanguage::Dxil:
                case ShadingLanguage::SpirV:
                    return binaryResult;

                case ShadingLanguage::Hlsl:
                case ShadingLanguage::Glsl:
                case ShadingLanguage::Essl:
                case ShadingLanguage::Msl_macOS:
                case ShadingLanguage::Msl_iOS:
                    return CrossCompile(binaryResult, source, options, target);

                default:
                    llvm_unreachable("Invalid shading language.");
                    break;
                }
            }
        }
        else
        {
            return binaryResult;
        }
    }
} // namespace

namespace ShaderConductor
{
    class Blob::BlobImpl
    {
    public:
        BlobImpl(const void* data, uint32_t size) noexcept
            : m_data(reinterpret_cast<const uint8_t*>(data), reinterpret_cast<const uint8_t*>(data) + size)
        {
        }

        const void* Data() const noexcept
        {
            return m_data.data();
        }

        uint32_t Size() const noexcept
        {
            return static_cast<uint32_t>(m_data.size());
        }

    private:
        std::vector<uint8_t> m_data;
    };

    Blob::Blob() noexcept = default;

    Blob::Blob(const void* data, uint32_t size)
    {
        this->Reset(data, size);
    }

    Blob::Blob(const Blob& other)
    {
        this->Reset(other.Data(), other.Size());
    }

    Blob::Blob(Blob&& other) noexcept : m_impl(std::move(other.m_impl))
    {
        other.m_impl = nullptr;
    }

    Blob::~Blob() noexcept
    {
        delete m_impl;
    }

    Blob& Blob::operator=(const Blob& other)
    {
        if (this != &other)
        {
            this->Reset(other.Data(), other.Size());
        }
        return *this;
    }

    Blob& Blob::operator=(Blob&& other) noexcept
    {
        if (this != &other)
        {
            delete m_impl;
            m_impl = std::move(other.m_impl);
            other.m_impl = nullptr;
        }
        return *this;
    }

    void Blob::Reset()
    {
        delete m_impl;
        m_impl = nullptr;
    }

    void Blob::Reset(const void* data, uint32_t size)
    {
        this->Reset();
        if ((data != nullptr) && (size > 0))
        {
            m_impl = new BlobImpl(data, size);
        }
    }

    const void* Blob::Data() const noexcept
    {
        return m_impl ? m_impl->Data() : nullptr;
    }

    uint32_t Blob::Size() const noexcept
    {
        return m_impl ? m_impl->Size() : 0;
    }


    class Reflection::VariableType::VariableTypeImpl
    {
    public:
#ifdef LLVM_ON_WIN32
        explicit VariableTypeImpl(ID3D12ShaderReflectionType* d3d12Type)
        {
            D3D12_SHADER_TYPE_DESC d3d12ShaderTypeDesc;
            d3d12Type->GetDesc(&d3d12ShaderTypeDesc);

            if (d3d12ShaderTypeDesc.Name)
            {
                m_name = d3d12ShaderTypeDesc.Name;
            }

            switch (d3d12ShaderTypeDesc.Type)
            {
            case D3D_SVT_BOOL:
                m_type = DataType::Bool;
                break;
            case D3D_SVT_INT:
                m_type = DataType::Int;
                break;
            case D3D_SVT_UINT:
                m_type = DataType::Uint;
                break;
            case D3D_SVT_FLOAT:
                m_type = DataType::Float;
                break;

            case D3D_SVT_MIN16FLOAT:
                m_type = DataType::Half;
                break;
            case D3D_SVT_MIN16INT:
                m_type = DataType::Int16;
                break;
            case D3D_SVT_MIN16UINT:
                m_type = DataType::Uint16;
                break;

            case D3D_SVT_VOID:
                if (d3d12ShaderTypeDesc.Class == D3D_SVC_STRUCT)
                {
                    m_type = DataType::Struct;

                    for (uint32_t memberIndex = 0; memberIndex < d3d12ShaderTypeDesc.Members; ++memberIndex)
                    {
                        VariableDesc member{};

                        const char* memberName = d3d12Type->GetMemberTypeName(memberIndex);
                        std::strncpy(member.name, memberName, sizeof(member.name));

                        ID3D12ShaderReflectionType* d3d12MemberType = d3d12Type->GetMemberTypeByIndex(memberIndex);
                        member.type = Make(d3d12MemberType);

                        D3D12_SHADER_TYPE_DESC d3d12MemberTypeDesc;
                        d3d12MemberType->GetDesc(&d3d12MemberTypeDesc);

                        member.offset = d3d12MemberTypeDesc.Offset;
                        if (d3d12MemberTypeDesc.Elements == 0)
                        {
                            member.size = 0;
                        }
                        else
                        {
                            member.size = (member.type.Elements() - 1) * member.type.ElementStride();
                        }
                        member.size += member.type.Rows() * member.type.Columns() * 4;

                        m_members.emplace_back(std::move(member));
                    }
                }
                else
                {
                    m_type = DataType::Void;
                }
                break;

            default:
                llvm_unreachable("Unsupported variable type.");
            }

            m_rows = d3d12ShaderTypeDesc.Rows;
            m_columns = d3d12ShaderTypeDesc.Columns;
            m_elements = d3d12ShaderTypeDesc.Elements;

            if (m_elements > 0)
            {
                m_elementStride = m_rows * 16;
            }
            else
            {
                m_elementStride = 0;
            }
        }
#endif

        VariableTypeImpl(const spirv_cross::Compiler& compiler, const spirv_cross::SPIRType& spirvParentReflectionType,
                         uint32_t variableIndex, const spirv_cross::SPIRType& spirvReflectionType)
        {
            switch (spirvReflectionType.basetype)
            {
            case spirv_cross::SPIRType::Boolean:
                m_type = DataType::Bool;
                m_name = "bool";
                break;
            case spirv_cross::SPIRType::Int:
                m_type = DataType::Int;
                m_name = "int";
                break;
            case spirv_cross::SPIRType::UInt:
                m_type = DataType::Uint;
                m_name = "uint";
                break;
            case spirv_cross::SPIRType::Float:
                m_type = DataType::Float;
                m_name = "float";
                break;

            case spirv_cross::SPIRType::Half:
                m_type = DataType::Half;
                m_name = "half";
                break;
            case spirv_cross::SPIRType::Short:
                m_type = DataType::Int16;
                m_name = "int16_t";
                break;
            case spirv_cross::SPIRType::UShort:
                m_type = DataType::Uint16;
                m_name = "uint16_t";
                break;

            case spirv_cross::SPIRType::Struct:
                m_type = DataType::Struct;
                m_name = compiler.get_name(spirvReflectionType.self);

                for (uint32_t memberIndex = 0; memberIndex < spirvReflectionType.member_types.size(); ++memberIndex)
                {
                    VariableDesc member{};

                    const std::string& memberName = compiler.get_member_name(spirvReflectionType.self, memberIndex);
                    std::strncpy(member.name, memberName.c_str(), sizeof(member.name));

                    member.type = Make(compiler, spirvReflectionType, memberIndex, compiler.get_type(spirvReflectionType.member_types[memberIndex]));

                    member.offset = compiler.type_struct_member_offset(spirvReflectionType, memberIndex);
                    member.size = static_cast<uint32_t>(compiler.get_declared_struct_member_size(spirvReflectionType, memberIndex));

                    m_members.emplace_back(std::move(member));
                }
                break;

            case spirv_cross::SPIRType::Void:
                m_type = DataType::Void;
                m_name = "void";
                break;

            default:
                llvm_unreachable("Unsupported variable type.");
                break;
            }

            if (spirvReflectionType.columns == 1)
            {
                if (spirvReflectionType.vecsize > 1)
                {
                    m_name += std::to_string(spirvReflectionType.vecsize);
                }
            }
            else
            {
                m_name += std::to_string(spirvReflectionType.columns) + 'x' + std::to_string(spirvReflectionType.vecsize);
            }

            m_rows = spirvReflectionType.columns;
            m_columns = spirvReflectionType.vecsize;
            if (compiler.has_member_decoration(spirvParentReflectionType.self, variableIndex, spv::DecorationColMajor))
            {
                std::swap(m_rows, m_columns);
            }
            if (spirvReflectionType.array.empty())
            {
                m_elements = 0;
            }
            else
            {
                m_elements = spirvReflectionType.array[0];
            }

            if (!spirvReflectionType.array.empty())
            {
                m_elementStride = compiler.type_struct_member_array_stride(spirvParentReflectionType, variableIndex);
            }
            else
            {
                m_elementStride = 0;
            }
        }

        const char* Name() const noexcept
        {
            return m_name.c_str();
        }

        DataType Type() const noexcept
        {
            return m_type;
        }

        uint32_t Rows() const noexcept
        {
            return m_rows;
        }

        uint32_t Columns() const noexcept
        {
            return m_columns;
        }

        uint32_t Elements() const noexcept
        {
            return m_elements;
        }

        uint32_t ElementStride() const noexcept
        {
            return m_elementStride;
        }

        uint32_t NumMembers() const noexcept
        {
            return static_cast<uint32_t>(m_members.size());
        }

        const VariableDesc* MemberByIndex(uint32_t index) const noexcept
        {
            if (index < m_members.size())
            {
                return &m_members[index];
            }

            return nullptr;
        }

        const VariableDesc* MemberByName(const char* name) const noexcept
        {
            for (const auto& member : m_members)
            {
                if (std::strcmp(member.name, name) == 0)
                {
                    return &member;
                }
            }

            return nullptr;
        }

#ifdef LLVM_ON_WIN32
        static VariableType Make(ID3D12ShaderReflectionType* d3d12ReflectionType)
        {
            VariableType ret;
            ret.m_impl = new VariableTypeImpl(d3d12ReflectionType);
            return ret;
        }
#endif

        static VariableType Make(const spirv_cross::Compiler& compiler, const spirv_cross::SPIRType& spirvParentReflectionType, uint32_t variableIndex,
                                 const spirv_cross::SPIRType& spirvReflectionType)
        {
            VariableType ret;
            ret.m_impl = new VariableTypeImpl(compiler, spirvParentReflectionType, variableIndex, spirvReflectionType);
            return ret;
        }

    private:
        std::string m_name;
        DataType m_type;
        uint32_t m_rows;
        uint32_t m_columns;
        uint32_t m_elements;
        uint32_t m_elementStride;

        std::vector<VariableDesc> m_members;
    };

    Reflection::VariableType::VariableType() noexcept = default;

    Reflection::VariableType::VariableType(const VariableType& other) : m_impl(other.m_impl ? new VariableTypeImpl(*other.m_impl) : nullptr)
    {
    }

    Reflection::VariableType::VariableType(VariableType&& other) noexcept : m_impl(std::move(other.m_impl))
    {
        other.m_impl = nullptr;
    }

    Reflection::VariableType::~VariableType() noexcept
    {
        delete m_impl;
    }

    Reflection::VariableType& Reflection::VariableType::operator=(const VariableType& other)
    {
        if (this != &other)
        {
            delete m_impl;
            m_impl = nullptr;

            if (other.m_impl)
            {
                m_impl = new VariableTypeImpl(*other.m_impl);
            }
        }
        return *this;
    }

    Reflection::VariableType& Reflection::VariableType::operator=(VariableType&& other) noexcept
    {
        if (this != &other)
        {
            delete m_impl;
            m_impl = std::move(other.m_impl);
            other.m_impl = nullptr;
        }
        return *this;
    }

    bool Reflection::VariableType::Valid() const noexcept
    {
        return m_impl != nullptr;
    }

    const char* Reflection::VariableType::Name() const noexcept
    {
        return m_impl->Name();
    }

    Reflection::VariableType::DataType Reflection::VariableType::Type() const noexcept
    {
        return m_impl->Type();
    }

    uint32_t Reflection::VariableType::Rows() const noexcept
    {
        return m_impl->Rows();
    }

    uint32_t Reflection::VariableType::Columns() const noexcept
    {
        return m_impl->Columns();
    }

    uint32_t Reflection::VariableType::Elements() const noexcept
    {
        return m_impl->Elements();
    }

    uint32_t Reflection::VariableType::ElementStride() const noexcept
    {
        return m_impl->ElementStride();
    }

    uint32_t Reflection::VariableType::NumMembers() const noexcept
    {
        return m_impl->NumMembers();
    }

    const Reflection::VariableDesc* Reflection::VariableType::MemberByIndex(uint32_t index) const noexcept
    {
        return m_impl->MemberByIndex(index);
    }

    const Reflection::VariableDesc* Reflection::VariableType::MemberByName(const char* name) const noexcept
    {
        return m_impl->MemberByName(name);
    }


    class Reflection::ConstantBuffer::ConstantBufferImpl
    {
    public:
#ifdef LLVM_ON_WIN32
        explicit ConstantBufferImpl(ID3D12ShaderReflectionConstantBuffer* constantBuffer)
        {
            D3D12_SHADER_BUFFER_DESC bufferDesc;
            constantBuffer->GetDesc(&bufferDesc);

            m_name = bufferDesc.Name;

            for (uint32_t variableIndex = 0; variableIndex < bufferDesc.Variables; ++variableIndex)
            {
                ID3D12ShaderReflectionVariable* variable = constantBuffer->GetVariableByIndex(variableIndex);
                D3D12_SHADER_VARIABLE_DESC d3d12VariableDesc;
                variable->GetDesc(&d3d12VariableDesc);

                VariableDesc variableDesc{};

                std::strncpy(variableDesc.name, d3d12VariableDesc.Name, sizeof(variableDesc.name));

                variableDesc.type = VariableType::VariableTypeImpl::Make(variable->GetType());

                variableDesc.offset = d3d12VariableDesc.StartOffset;
                variableDesc.size = d3d12VariableDesc.Size;

                m_variableDescs.emplace_back(std::move(variableDesc));
            }

            m_size = bufferDesc.Size;
        }
#endif

        explicit ConstantBufferImpl(const spirv_cross::Compiler& compiler, const spirv_cross::Resource& resource)
        {
            const auto& cbufferType = compiler.get_type(resource.type_id);

            m_name = compiler.get_name(resource.id);

            for (uint32_t variableIndex = 0; variableIndex < cbufferType.member_types.size(); ++variableIndex)
            {
                VariableDesc variableDesc{};

                const std::string& varName = compiler.get_member_name(cbufferType.self, variableIndex);
                std::strncpy(variableDesc.name, varName.c_str(), sizeof(variableDesc.name));

                variableDesc.type = VariableType::VariableTypeImpl::Make(compiler, cbufferType, variableIndex,
                                                                         compiler.get_type(cbufferType.member_types[variableIndex]));

                variableDesc.offset = compiler.type_struct_member_offset(cbufferType, variableIndex);
                variableDesc.size = static_cast<uint32_t>(compiler.get_declared_struct_member_size(cbufferType, variableIndex));

                m_variableDescs.emplace_back(std::move(variableDesc));
            }

            m_size = static_cast<uint32_t>(compiler.get_declared_struct_size(cbufferType));
        }

        const char* Name() const noexcept
        {
            return m_name.c_str();
        }

        uint32_t Size() const noexcept
        {
            return m_size;
        }

        uint32_t NumVariables() const noexcept
        {
            return static_cast<uint32_t>(m_variableDescs.size());
        }

        const VariableDesc* VariableByIndex(uint32_t index) const noexcept
        {
            if (index < m_variableDescs.size())
            {
                return &m_variableDescs[index];
            }

            return nullptr;
        }

        const VariableDesc* VariableByName(const char* name) const noexcept
        {
            for (const auto& variableDesc : m_variableDescs)
            {
                if (std::strcmp(variableDesc.name, name) == 0)
                {
                    return &variableDesc;
                }
            }

            return nullptr;
        }

#ifdef LLVM_ON_WIN32
        static ConstantBuffer Make(ID3D12ShaderReflectionConstantBuffer* constantBuffer)
        {
            ConstantBuffer ret;
            ret.m_impl = new ConstantBufferImpl(constantBuffer);
            return ret;
        }
#endif

        static ConstantBuffer Make(const spirv_cross::Compiler& compiler, const spirv_cross::Resource& resource)
        {
            ConstantBuffer ret;
            ret.m_impl = new ConstantBufferImpl(compiler, resource);
            return ret;
        }

    private:
        std::string m_name;
        uint32_t m_size;
        std::vector<VariableDesc> m_variableDescs;
    };

    Reflection::ConstantBuffer::ConstantBuffer() noexcept = default;

    Reflection::ConstantBuffer::ConstantBuffer(const ConstantBuffer& other)
        : m_impl(other.m_impl ? new ConstantBufferImpl(*other.m_impl) : nullptr)
    {
    }

    Reflection::ConstantBuffer::ConstantBuffer(ConstantBuffer&& other) noexcept : m_impl(std::move(other.m_impl))
    {
        other.m_impl = nullptr;
    }

    Reflection::ConstantBuffer::~ConstantBuffer() noexcept
    {
        delete m_impl;
    }

    Reflection::ConstantBuffer& Reflection::ConstantBuffer::operator=(const ConstantBuffer& other)
    {
        if (this != &other)
        {
            delete m_impl;
            m_impl = nullptr;

            if (other.m_impl)
            {
                m_impl = new ConstantBufferImpl(*other.m_impl);
            }
        }
        return *this;
    }

    Reflection::ConstantBuffer& Reflection::ConstantBuffer::operator=(ConstantBuffer&& other) noexcept
    {
        if (this != &other)
        {
            delete m_impl;
            m_impl = std::move(other.m_impl);
            other.m_impl = nullptr;
        }
        return *this;
    }

    bool Reflection::ConstantBuffer::Valid() const noexcept
    {
        return m_impl != nullptr;
    }

    const char* Reflection::ConstantBuffer::Name() const noexcept
    {
        return m_impl->Name();
    }

    uint32_t Reflection::ConstantBuffer::Size() const noexcept
    {
        return m_impl->Size();
    }

    uint32_t Reflection::ConstantBuffer::NumVariables() const noexcept
    {
        return m_impl->NumVariables();
    }

    const Reflection::VariableDesc* Reflection::ConstantBuffer::VariableByIndex(uint32_t index) const noexcept
    {
        return m_impl->VariableByIndex(index);
    }

    const Reflection::VariableDesc* Reflection::ConstantBuffer::VariableByName(const char* name) const noexcept
    {
        return m_impl->VariableByName(name);
    }


    class Reflection::ReflectionImpl
    {
    public:
#ifdef LLVM_ON_WIN32
        explicit ReflectionImpl(IDxcBlob* dxilBlob)
        {
            CComPtr<ID3D12ShaderReflection> shaderReflection;
            IFT(CreateDxcReflectionFromBlob(dxilBlob, shaderReflection));

            D3D12_SHADER_DESC shaderDesc;
            shaderReflection->GetDesc(&shaderDesc);

            for (uint32_t resourceIndex = 0; resourceIndex < shaderDesc.BoundResources; ++resourceIndex)
            {
                D3D12_SHADER_INPUT_BIND_DESC bindDesc;
                shaderReflection->GetResourceBindingDesc(resourceIndex, &bindDesc);

                ResourceDesc reflectionDesc{};

                std::strncpy(reflectionDesc.name, bindDesc.Name, sizeof(reflectionDesc.name));
                reflectionDesc.space = bindDesc.Space;
                reflectionDesc.bindPoint = bindDesc.BindPoint;
                reflectionDesc.bindCount = bindDesc.BindCount;

                if (bindDesc.Type == D3D_SIT_CBUFFER || bindDesc.Type == D3D_SIT_TBUFFER)
                {
                    reflectionDesc.type = ShaderResourceType::ConstantBuffer;

                    ID3D12ShaderReflectionConstantBuffer* constantBuffer = shaderReflection->GetConstantBufferByName(bindDesc.Name);
                    m_constantBuffers.emplace_back(ConstantBuffer::ConstantBufferImpl::Make(constantBuffer));
                }
                else
                {
                    switch (bindDesc.Type)
                    {
                    case D3D_SIT_TEXTURE:
                        reflectionDesc.type = ShaderResourceType::Texture;
                        break;

                    case D3D_SIT_SAMPLER:
                        reflectionDesc.type = ShaderResourceType::Sampler;
                        break;

                    case D3D_SIT_STRUCTURED:
                    case D3D_SIT_BYTEADDRESS:
                        reflectionDesc.type = ShaderResourceType::ShaderResourceView;
                        break;

                    case D3D_SIT_UAV_RWTYPED:
                    case D3D_SIT_UAV_RWSTRUCTURED:
                    case D3D_SIT_UAV_RWBYTEADDRESS:
                    case D3D_SIT_UAV_APPEND_STRUCTURED:
                    case D3D_SIT_UAV_CONSUME_STRUCTURED:
                    case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
                        reflectionDesc.type = ShaderResourceType::UnorderedAccessView;
                        break;

                    default:
                        llvm_unreachable("Unknown bind type.");
                        break;
                    }
                }

                m_resourceDescs.emplace_back(std::move(reflectionDesc));
            }

            for (uint32_t inputParamIndex = 0; inputParamIndex < shaderDesc.InputParameters; ++inputParamIndex)
            {
                SignatureParameterDesc paramDesc{};

                D3D12_SIGNATURE_PARAMETER_DESC signatureParamDesc;
                shaderReflection->GetInputParameterDesc(inputParamIndex, &signatureParamDesc);

                std::strncpy(paramDesc.semantic, signatureParamDesc.SemanticName, sizeof(paramDesc.semantic));
                paramDesc.semanticIndex = signatureParamDesc.SemanticIndex;
                paramDesc.location = signatureParamDesc.Register;
                switch (signatureParamDesc.ComponentType)
                {
                case D3D_REGISTER_COMPONENT_UINT32:
                    paramDesc.componentType = VariableType::DataType::Uint;
                    break;
                case D3D_REGISTER_COMPONENT_SINT32:
                    paramDesc.componentType = VariableType::DataType::Int;
                    break;
                case D3D_REGISTER_COMPONENT_FLOAT32:
                    paramDesc.componentType = VariableType::DataType::Float;
                    break;

                default:
                    llvm_unreachable("Unsupported input component type.");
                    break;
                }
                paramDesc.mask = static_cast<ComponentMask>(signatureParamDesc.Mask);

                m_inputParams.emplace_back(std::move(paramDesc));
            }

            for (uint32_t outputParamIndex = 0; outputParamIndex < shaderDesc.OutputParameters; ++outputParamIndex)
            {
                SignatureParameterDesc paramDesc{};

                D3D12_SIGNATURE_PARAMETER_DESC signatureParamDesc;
                shaderReflection->GetOutputParameterDesc(outputParamIndex, &signatureParamDesc);

                std::strncpy(paramDesc.semantic, signatureParamDesc.SemanticName, sizeof(paramDesc.semantic));
                paramDesc.semanticIndex = signatureParamDesc.SemanticIndex;
                paramDesc.location = signatureParamDesc.Register;
                switch (signatureParamDesc.ComponentType)
                {
                case D3D_REGISTER_COMPONENT_UINT32:
                    paramDesc.componentType = VariableType::DataType::Uint;
                    break;
                case D3D_REGISTER_COMPONENT_SINT32:
                    paramDesc.componentType = VariableType::DataType::Int;
                    break;
                case D3D_REGISTER_COMPONENT_FLOAT32:
                    paramDesc.componentType = VariableType::DataType::Float;
                    break;

                default:
                    llvm_unreachable("Unsupported output component type.");
                    break;
                }
                paramDesc.mask = static_cast<ComponentMask>(signatureParamDesc.Mask);

                m_outputParams.emplace_back(std::move(paramDesc));
            }

            switch (shaderDesc.InputPrimitive)
            {
            case D3D_PRIMITIVE_UNDEFINED:
                m_gsHSInputPrimitive = PrimitiveTopology::Undefined;
                break;
            case D3D_PRIMITIVE_POINT:
                m_gsHSInputPrimitive = PrimitiveTopology::Points;
                break;
            case D3D_PRIMITIVE_LINE:
                m_gsHSInputPrimitive = PrimitiveTopology::Lines;
                break;
            case D3D_PRIMITIVE_TRIANGLE:
                m_gsHSInputPrimitive = PrimitiveTopology::Triangles;
                break;
            case D3D_PRIMITIVE_LINE_ADJ:
                m_gsHSInputPrimitive = PrimitiveTopology::LinesAdj;
                break;
            case D3D_PRIMITIVE_TRIANGLE_ADJ:
                m_gsHSInputPrimitive = PrimitiveTopology::TrianglesAdj;
                break;
            case D3D_PRIMITIVE_1_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_1_CtrlPoint;
                break;
            case D3D_PRIMITIVE_2_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_2_CtrlPoint;
                break;
            case D3D_PRIMITIVE_3_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_3_CtrlPoint;
                break;
            case D3D_PRIMITIVE_4_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_4_CtrlPoint;
                break;
            case D3D_PRIMITIVE_5_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_5_CtrlPoint;
                break;
            case D3D_PRIMITIVE_6_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_6_CtrlPoint;
                break;
            case D3D_PRIMITIVE_7_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_7_CtrlPoint;
                break;
            case D3D_PRIMITIVE_8_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_8_CtrlPoint;
                break;
            case D3D_PRIMITIVE_9_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_9_CtrlPoint;
                break;
            case D3D_PRIMITIVE_10_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_10_CtrlPoint;
                break;
            case D3D_PRIMITIVE_11_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_11_CtrlPoint;
                break;
            case D3D_PRIMITIVE_12_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_12_CtrlPoint;
                break;
            case D3D_PRIMITIVE_13_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_13_CtrlPoint;
                break;
            case D3D_PRIMITIVE_14_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_14_CtrlPoint;
                break;
            case D3D_PRIMITIVE_15_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_15_CtrlPoint;
                break;
            case D3D_PRIMITIVE_16_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_16_CtrlPoint;
                break;
            case D3D_PRIMITIVE_17_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_17_CtrlPoint;
                break;
            case D3D_PRIMITIVE_18_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_18_CtrlPoint;
                break;
            case D3D_PRIMITIVE_19_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_19_CtrlPoint;
                break;
            case D3D_PRIMITIVE_20_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_20_CtrlPoint;
                break;
            case D3D_PRIMITIVE_21_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_21_CtrlPoint;
                break;
            case D3D_PRIMITIVE_22_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_22_CtrlPoint;
                break;
            case D3D_PRIMITIVE_23_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_23_CtrlPoint;
                break;
            case D3D_PRIMITIVE_24_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_24_CtrlPoint;
                break;
            case D3D_PRIMITIVE_25_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_25_CtrlPoint;
                break;
            case D3D_PRIMITIVE_26_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_26_CtrlPoint;
                break;
            case D3D_PRIMITIVE_27_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_27_CtrlPoint;
                break;
            case D3D_PRIMITIVE_28_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_28_CtrlPoint;
                break;
            case D3D_PRIMITIVE_29_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_29_CtrlPoint;
                break;
            case D3D_PRIMITIVE_30_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_30_CtrlPoint;
                break;
            case D3D_PRIMITIVE_31_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_31_CtrlPoint;
                break;
            case D3D_PRIMITIVE_32_CONTROL_POINT_PATCH:
                m_gsHSInputPrimitive = PrimitiveTopology::Patches_32_CtrlPoint;
                break;

            default:
                llvm_unreachable("Unsupported input primitive type.");
                break;
            }

            switch (shaderDesc.GSOutputTopology)
            {
            case D3D_PRIMITIVE_TOPOLOGY_UNDEFINED:
                m_gsOutputTopology = PrimitiveTopology::Undefined;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
                m_gsOutputTopology = PrimitiveTopology::Points;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
                m_gsOutputTopology = PrimitiveTopology::Lines;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
                m_gsOutputTopology = PrimitiveTopology::LineStrip;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
                m_gsOutputTopology = PrimitiveTopology::Triangles;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
                m_gsOutputTopology = PrimitiveTopology::TriangleStrip;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
                m_gsOutputTopology = PrimitiveTopology::LinesAdj;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
                m_gsOutputTopology = PrimitiveTopology::LineStripAdj;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
                m_gsOutputTopology = PrimitiveTopology::TrianglesAdj;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
                m_gsOutputTopology = PrimitiveTopology::TriangleStripAdj;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_1_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_2_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_3_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_4_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_5_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_6_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_7_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_8_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_9_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_10_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_11_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_12_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_13_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_14_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_15_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_16_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_17_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_18_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_19_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_20_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_21_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_22_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_23_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_24_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_25_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_26_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_27_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_28_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_29_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_30_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_31_CtrlPoint;
                break;
            case D3D_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST:
                m_gsOutputTopology = PrimitiveTopology::Patches_32_CtrlPoint;
                break;

            default:
                llvm_unreachable("Unsupported output topoloty type.");
                break;
            }

            m_gsMaxNumOutputVertices = shaderDesc.GSMaxOutputVertexCount;
            m_gsNumInstances = shaderDesc.cGSInstanceCount;

            switch (shaderDesc.HSOutputPrimitive)
            {
            case D3D_TESSELLATOR_OUTPUT_UNDEFINED:
                m_hsOutputPrimitive = TessellatorOutputPrimitive::Undefined;
                break;
            case D3D_TESSELLATOR_OUTPUT_POINT:
                m_hsOutputPrimitive = TessellatorOutputPrimitive::Point;
                break;
            case D3D_TESSELLATOR_OUTPUT_LINE:
                m_hsOutputPrimitive = TessellatorOutputPrimitive::Line;
                break;
            case D3D_TESSELLATOR_OUTPUT_TRIANGLE_CW:
                m_hsOutputPrimitive = TessellatorOutputPrimitive::TriangleCW;
                break;
            case D3D_TESSELLATOR_OUTPUT_TRIANGLE_CCW:
                m_hsOutputPrimitive = TessellatorOutputPrimitive::TriangleCCW;
                break;

            default:
                llvm_unreachable("Unsupported output primitive type.");
                break;
            }

            switch (shaderDesc.HSPartitioning)
            {
            case D3D_TESSELLATOR_PARTITIONING_UNDEFINED:
                m_hsPartitioning = TessellatorPartitioning::Undefined;
                break;
            case D3D_TESSELLATOR_PARTITIONING_INTEGER:
                m_hsPartitioning = TessellatorPartitioning::Integer;
                break;
            case D3D_TESSELLATOR_PARTITIONING_POW2:
                m_hsPartitioning = TessellatorPartitioning::Pow2;
                break;
            case D3D_TESSELLATOR_PARTITIONING_FRACTIONAL_ODD:
                m_hsPartitioning = TessellatorPartitioning::FractionalOdd;
                break;
            case D3D_TESSELLATOR_PARTITIONING_FRACTIONAL_EVEN:
                m_hsPartitioning = TessellatorPartitioning::FractionalEven;
                break;

            default:
                llvm_unreachable("Unsupported partitioning type.");
                break;
            }

            switch (shaderDesc.TessellatorDomain)
            {
            case D3D_TESSELLATOR_DOMAIN_UNDEFINED:
                m_hSDSTessellatorDomain = TessellatorDomain::Undefined;
                break;
            case D3D_TESSELLATOR_DOMAIN_ISOLINE:
                m_hSDSTessellatorDomain = TessellatorDomain::Line;
                break;
            case D3D_TESSELLATOR_DOMAIN_TRI:
                m_hSDSTessellatorDomain = TessellatorDomain::Triangle;
                break;
            case D3D_TESSELLATOR_DOMAIN_QUAD:
                m_hSDSTessellatorDomain = TessellatorDomain::Quad;
                break;

            default:
                llvm_unreachable("Unsupported tessellator domain type.");
                break;
            }

            for (uint32_t patchConstantParamIndex = 0; patchConstantParamIndex < shaderDesc.PatchConstantParameters;
                 ++patchConstantParamIndex)
            {
                SignatureParameterDesc paramDesc{};

                D3D12_SIGNATURE_PARAMETER_DESC signatureParamDesc;
                shaderReflection->GetPatchConstantParameterDesc(patchConstantParamIndex, &signatureParamDesc);

                std::strncpy(paramDesc.semantic, signatureParamDesc.SemanticName, sizeof(paramDesc.semantic));
                paramDesc.semanticIndex = signatureParamDesc.SemanticIndex;
                paramDesc.location = signatureParamDesc.Register;
                switch (signatureParamDesc.ComponentType)
                {
                case D3D_REGISTER_COMPONENT_UINT32:
                    paramDesc.componentType = VariableType::DataType::Uint;
                    break;
                case D3D_REGISTER_COMPONENT_SINT32:
                    paramDesc.componentType = VariableType::DataType::Int;
                    break;
                case D3D_REGISTER_COMPONENT_FLOAT32:
                    paramDesc.componentType = VariableType::DataType::Float;
                    break;

                default:
                    llvm_unreachable("Unsupported patch constant component type.");
                    break;
                }
                paramDesc.mask = static_cast<ComponentMask>(signatureParamDesc.Mask);

                m_hsDSPatchConstantParams.emplace_back(std::move(paramDesc));
            }

            m_hsDSNumCtrlPoints = shaderDesc.cControlPoints;

            shaderReflection->GetThreadGroupSize(&m_csBlockSizeX, &m_csBlockSizeY, &m_csBlockSizeZ);
        }
#endif

        explicit ReflectionImpl(const spirv_cross::Compiler& compiler)
        {
            spirv_cross::ShaderResources resources = compiler.get_shader_resources();

            for (const auto& resource : resources.uniform_buffers)
            {
                ResourceDesc reflectionDesc{};
                this->ExtractReflection(reflectionDesc, compiler, resource.id);
                reflectionDesc.type = ShaderResourceType::ConstantBuffer;

                m_resourceDescs.emplace_back(std::move(reflectionDesc));

                m_constantBuffers.emplace_back(ConstantBuffer::ConstantBufferImpl::Make(compiler, resource));
            }

            for (const auto& resource : resources.storage_buffers)
            {
                ResourceDesc reflectionDesc{};
                this->ExtractReflection(reflectionDesc, compiler, resource.id);

                const spirv_cross::Bitset& typeFlags = compiler.get_decoration_bitset(compiler.get_type(resource.type_id).self);
                const auto& type = compiler.get_type(resource.type_id);

                const bool ssboBlock = type.storage == spv::StorageClassStorageBuffer ||
                                       (type.storage == spv::StorageClassUniform && typeFlags.get(spv::DecorationBufferBlock));
                if (ssboBlock)
                {
                    spirv_cross::Bitset buffer_flags = compiler.get_buffer_block_flags(resource.id);
                    if (buffer_flags.get(spv::DecorationNonWritable))
                    {
                        reflectionDesc.type = ShaderResourceType::ShaderResourceView;
                    }
                    else
                    {
                        reflectionDesc.type = ShaderResourceType::UnorderedAccessView;
                    }
                }
                else
                {
                    reflectionDesc.type = ShaderResourceType::ShaderResourceView;
                }

                m_resourceDescs.emplace_back(std::move(reflectionDesc));
            }

            for (const auto& resource : resources.storage_images)
            {
                ResourceDesc reflectionDesc{};
                this->ExtractReflection(reflectionDesc, compiler, resource.id);
                reflectionDesc.type = ShaderResourceType::UnorderedAccessView;

                m_resourceDescs.emplace_back(std::move(reflectionDesc));
            }

            for (const auto& resource : resources.separate_images)
            {
                ResourceDesc reflectionDesc{};
                this->ExtractReflection(reflectionDesc, compiler, resource.id);
                reflectionDesc.type = ShaderResourceType::Texture;

                m_resourceDescs.emplace_back(std::move(reflectionDesc));
            }

            for (const auto& resource : resources.separate_samplers)
            {
                ResourceDesc reflectionDesc{};
                this->ExtractReflection(reflectionDesc, compiler, resource.id);
                reflectionDesc.type = ShaderResourceType::Sampler;

                m_resourceDescs.emplace_back(std::move(reflectionDesc));
            }

            uint32_t combinedBinding = 0;
            for (const auto& resource : resources.sampled_images)
            {
                ResourceDesc reflectionDesc{};
                this->ExtractReflection(reflectionDesc, compiler, resource.id);
                reflectionDesc.bindPoint = combinedBinding;
                reflectionDesc.type = ShaderResourceType::Texture;

                m_resourceDescs.emplace_back(std::move(reflectionDesc));
                ++combinedBinding;
            }

            for (const auto& inputParam : resources.builtin_inputs)
            {
                const std::string semantic = this->ExtractBuiltInemantic(inputParam.builtin);
                if (!semantic.empty())
                {
                    SignatureParameterDesc paramDesc{};
                    this->ExtractParameter(paramDesc, compiler, inputParam.resource, semantic);

                    const auto& type = compiler.get_type(inputParam.resource.type_id);
                    switch (inputParam.builtin)
                    {
                    case spv::BuiltInTessLevelInner:
                    case spv::BuiltInTessLevelOuter:
                        for (uint32_t patchConstantParamIndex = 0; patchConstantParamIndex < type.array[0]; ++patchConstantParamIndex)
                        {
                            if (inputParam.builtin == spv::BuiltInTessLevelOuter)
                            {
                                paramDesc.semanticIndex = patchConstantParamIndex;
                                paramDesc.mask = ComponentMask::W;
                            }
                            else
                            {
                                paramDesc.semanticIndex = 0;
                                paramDesc.mask = ComponentMask::X;
                            }

                            m_hsDSPatchConstantParams.emplace_back(std::move(paramDesc));
                        }
                        break;

                    default:
                        m_inputParams.emplace_back(std::move(paramDesc));
                        break;
                    }
                }
            }

            for (const auto& inputParam : resources.stage_inputs)
            {
                SignatureParameterDesc paramDesc{};
                this->ExtractParameter(paramDesc, compiler, inputParam);

                if (compiler.get_decoration(inputParam.id, spv::DecorationPatch))
                {
                    m_hsDSPatchConstantParams.emplace_back(std::move(paramDesc));
                }
                else
                {
                    m_inputParams.emplace_back(std::move(paramDesc));
                }
            }

            for (const auto& outputParam : resources.builtin_outputs)
            {
                const std::string semantic = this->ExtractBuiltInemantic(outputParam.builtin);
                if (!semantic.empty())
                {
                    SignatureParameterDesc paramDesc{};
                    const auto& type = compiler.get_type(outputParam.resource.type_id);
                    this->ExtractParameter(paramDesc, compiler, outputParam.resource, semantic);

                    switch (type.basetype)
                    {
                    case spirv_cross::SPIRType::UInt:
                        paramDesc.componentType = VariableType::DataType::Uint;
                        break;
                    case spirv_cross::SPIRType::Int:
                        paramDesc.componentType = VariableType::DataType::Int;
                        break;
                    case spirv_cross::SPIRType::Float:
                        paramDesc.componentType = VariableType::DataType::Float;
                        break;

                    default:
                        llvm_unreachable("Unsupported parameter component type.");
                        break;
                    }

                    if (type.vecsize > 0)
                    {
                        paramDesc.mask = ComponentMask::X;
                    }
                    if (type.vecsize > 1)
                    {
                        paramDesc.mask |= ComponentMask::Y;
                    }
                    if (type.vecsize > 2)
                    {
                        paramDesc.mask |= ComponentMask::Z;
                    }
                    if (type.vecsize > 3)
                    {
                        paramDesc.mask |= ComponentMask::W;
                    }

                    switch (outputParam.builtin)
                    {
                    case spv::BuiltInTessLevelInner:
                    case spv::BuiltInTessLevelOuter:
                        for (uint32_t patchConstantParamIndex = 0; patchConstantParamIndex < type.array[0]; ++patchConstantParamIndex)
                        {
                            paramDesc.semanticIndex = patchConstantParamIndex;

                            if (outputParam.builtin == spv::BuiltInTessLevelOuter)
                            {
                                paramDesc.mask = ComponentMask::W;
                            }
                            else
                            {
                                paramDesc.mask = ComponentMask::X;
                            }

                            m_hsDSPatchConstantParams.emplace_back(std::move(paramDesc));
                        }
                        break;

                    default:
                        m_outputParams.emplace_back(std::move(paramDesc));
                        break;
                    }
                }
            }

            for (const auto& outputParam : resources.stage_outputs)
            {
                SignatureParameterDesc paramDesc{};
                this->ExtractParameter(paramDesc, compiler, outputParam);

                m_outputParams.emplace_back(std::move(paramDesc));
            }

            m_gsHSInputPrimitive = Reflection::PrimitiveTopology::Undefined;
            m_gsOutputTopology = Reflection::PrimitiveTopology::Undefined;
            m_gsMaxNumOutputVertices = 0;
            m_gsNumInstances = 0;
            m_hsOutputPrimitive = Reflection::TessellatorOutputPrimitive::Undefined;
            m_hsPartitioning = Reflection::TessellatorPartitioning::Undefined;
            m_hSDSTessellatorDomain = Reflection::TessellatorDomain::Undefined;
            m_hsDSNumCtrlPoints = 0;
            m_csBlockSizeX = m_csBlockSizeY = m_csBlockSizeZ = 0;

            const auto& modes = compiler.get_execution_mode_bitset();
            switch (compiler.get_execution_model())
            {
            case spv::ExecutionModelTessellationControl:
                if (modes.get(spv::ExecutionModeOutputVertices))
                {
                    m_hsDSNumCtrlPoints = compiler.get_execution_mode_argument(spv::ExecutionModeOutputVertices, 0);
                    switch (m_hsDSNumCtrlPoints)
                    {
                    case 2:
                        m_hSDSTessellatorDomain = TessellatorDomain::Line;
                        break;
                    case 3:
                        m_hSDSTessellatorDomain = TessellatorDomain::Triangle;
                        break;
                    case 4:
                        m_hSDSTessellatorDomain = TessellatorDomain::Quad;
                        break;

                    default:
                        break;
                    }
                }

                if (modes.get(spv::ExecutionModeInputPoints))
                {
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_1_CtrlPoint;
                }
                else if (modes.get(spv::ExecutionModeInputLines))
                {
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_2_CtrlPoint;
                }
                else if (modes.get(spv::ExecutionModeTriangles))
                {
                    m_gsHSInputPrimitive = PrimitiveTopology::Patches_3_CtrlPoint;
                }

                if (modes.get(spv::ExecutionModeVertexOrderCw))
                {
                    m_hsOutputPrimitive = TessellatorOutputPrimitive::TriangleCW;
                }
                else if (modes.get(spv::ExecutionModeVertexOrderCcw))
                {
                    m_hsOutputPrimitive = TessellatorOutputPrimitive::TriangleCCW;
                }

                if (modes.get(spv::ExecutionModeSpacingEqual))
                {
                    m_hsPartitioning = TessellatorPartitioning::Integer;
                }
                else if (modes.get(spv::ExecutionModeSpacingFractionalOdd))
                {
                    m_hsPartitioning = TessellatorPartitioning::FractionalOdd;
                }
                else if (modes.get(spv::ExecutionModeSpacingFractionalOdd))
                {
                    m_hsPartitioning = TessellatorPartitioning::FractionalEven;
                }
                break;

            case spv::ExecutionModelTessellationEvaluation:
                if (modes.get(spv::ExecutionModeIsolines))
                {
                    m_hSDSTessellatorDomain = TessellatorDomain::Line;
                    m_hsDSNumCtrlPoints = 2;
                }
                else if (modes.get(spv::ExecutionModeTriangles))
                {
                    m_hSDSTessellatorDomain = TessellatorDomain::Triangle;
                    m_hsDSNumCtrlPoints = 3;
                }
                else if (modes.get(spv::ExecutionModeQuads))
                {
                    m_hSDSTessellatorDomain = TessellatorDomain::Quad;
                    m_hsDSNumCtrlPoints = 4;
                }
                break;

            case spv::ExecutionModelGeometry:
                if (modes.get(spv::ExecutionModeOutputVertices))
                {
                    m_gsMaxNumOutputVertices = compiler.get_execution_mode_argument(spv::ExecutionModeOutputVertices, 0);
                }

                if (modes.get(spv::ExecutionModeInvocations))
                {
                    m_gsNumInstances = compiler.get_execution_mode_argument(spv::ExecutionModeInvocations, 0);
                }

                if (modes.get(spv::ExecutionModeInputPoints))
                {
                    m_gsHSInputPrimitive = PrimitiveTopology::Points;
                }
                else if (modes.get(spv::ExecutionModeInputLines))
                {
                    m_gsHSInputPrimitive = PrimitiveTopology::Lines;
                }
                else if (modes.get(spv::ExecutionModeTriangles))
                {
                    m_gsHSInputPrimitive = PrimitiveTopology::Triangles;
                }
                else if (modes.get(spv::ExecutionModeInputLinesAdjacency))
                {
                    m_gsHSInputPrimitive = PrimitiveTopology::LinesAdj;
                }
                else if (modes.get(spv::ExecutionModeInputTrianglesAdjacency))
                {
                    m_gsHSInputPrimitive = PrimitiveTopology::TrianglesAdj;
                }

                if (modes.get(spv::ExecutionModeOutputPoints))
                {
                    m_gsOutputTopology = PrimitiveTopology::Points;
                }
                else if (modes.get(spv::ExecutionModeOutputLineStrip))
                {
                    m_gsOutputTopology = PrimitiveTopology::LineStrip;
                }
                else if (modes.get(spv::ExecutionModeOutputTriangleStrip))
                {
                    m_gsOutputTopology = PrimitiveTopology::TriangleStrip;
                }
                break;

            case spv::ExecutionModelGLCompute:
            {
                spirv_cross::SpecializationConstant spec_x, spec_y, spec_z;
                compiler.get_work_group_size_specialization_constants(spec_x, spec_y, spec_z);

                m_csBlockSizeX = spec_x.id != spirv_cross::ID(0) ? spec_x.constant_id
                                                                 : compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 0);
                m_csBlockSizeY = spec_y.id != spirv_cross::ID(0) ? spec_y.constant_id
                                                                 : compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 1);
                m_csBlockSizeZ = spec_z.id != spirv_cross::ID(0) ? spec_z.constant_id
                                                                 : compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 2);
                break;
            }

            default:
                break;
            }
        }

        uint32_t NumResources() const noexcept
        {
            return static_cast<uint32_t>(m_resourceDescs.size());
        }

        const ResourceDesc* ResourceByIndex(uint32_t index) const noexcept
        {
            if (index < m_resourceDescs.size())
            {
                return &m_resourceDescs[index];
            }

            return nullptr;
        }

        const ResourceDesc* ResourceByName(const char* name) const noexcept
        {
            for (const auto& resourceDesc : m_resourceDescs)
            {
                if (std::strcmp(resourceDesc.name, name) == 0)
                {
                    return &resourceDesc;
                }
            }

            return nullptr;
        }

        uint32_t NumConstantBuffers() const noexcept
        {
            return static_cast<uint32_t>(m_constantBuffers.size());
        }

        const ConstantBuffer* ConstantBufferByIndex(uint32_t index) const noexcept
        {
            if (index < m_resourceDescs.size())
            {
                return &m_constantBuffers[index];
            }

            return nullptr;
        }

        const ConstantBuffer* ConstantBufferByName(const char* name) const noexcept
        {
            for (const auto& cbuffer : m_constantBuffers)
            {
                if (std::strcmp(cbuffer.Name(), name) == 0)
                {
                    return &cbuffer;
                }
            }

            return nullptr;
        }

        uint32_t NumInputParameters() const noexcept
        {
            return static_cast<uint32_t>(m_inputParams.size());
        }

        const SignatureParameterDesc* InputParameter(uint32_t index) const noexcept
        {
            if (index < m_inputParams.size())
            {
                return &m_inputParams[index];
            }

            return nullptr;
        }

        uint32_t NumOutputParameters() const noexcept
        {
            return static_cast<uint32_t>(m_outputParams.size());
        }

        const SignatureParameterDesc* OutputParameter(uint32_t index) const noexcept
        {
            if (index < m_outputParams.size())
            {
                return &m_outputParams[index];
            }

            return nullptr;
        }

        PrimitiveTopology GSHSInputPrimitive() const noexcept
        {
            return m_gsHSInputPrimitive;
        }

        PrimitiveTopology GSOutputTopology() const noexcept
        {
            return m_gsOutputTopology;
        }

        uint32_t GSMaxNumOutputVertices() const noexcept
        {
            return m_gsMaxNumOutputVertices;
        }

        uint32_t GSNumInstances() const noexcept
        {
            return m_gsNumInstances;
        }

        TessellatorOutputPrimitive HSOutputPrimitive() const noexcept
        {
            return m_hsOutputPrimitive;
        }

        TessellatorPartitioning HSPartitioning() const noexcept
        {
            return m_hsPartitioning;
        }

        TessellatorDomain HSDSTessellatorDomain() const noexcept
        {
            return m_hSDSTessellatorDomain;
        }

        uint32_t HSDSNumPatchConstantParameters() const noexcept
        {
            return static_cast<uint32_t>(m_hsDSPatchConstantParams.size());
        }

        const SignatureParameterDesc* HSDSPatchConstantParameter(uint32_t index) const noexcept
        {
            if (index < m_hsDSPatchConstantParams.size())
            {
                return &m_hsDSPatchConstantParams[index];
            }

            return nullptr;
        }

        uint32_t HSDSNumConrolPoints() const noexcept
        {
            return m_hsDSNumCtrlPoints;
        }

        uint32_t CSBlockSizeX() const noexcept
        {
            return m_csBlockSizeX;
        }

        uint32_t CSBlockSizeY() const noexcept
        {
            return m_csBlockSizeY;
        }

        uint32_t CSBlockSizeZ() const noexcept
        {
            return m_csBlockSizeZ;
        }

#ifdef LLVM_ON_WIN32
        static Reflection Make(IDxcBlob* dxilBlob)
        {
            Reflection ret;
            ret.m_impl = new ReflectionImpl(dxilBlob);
            return ret;
        }
#endif

        static Reflection Make(const spirv_cross::Compiler& compiler)
        {
            Reflection ret;
            ret.m_impl = new ReflectionImpl(compiler);
            return ret;
        }

    private:
        static std::string ExtractBuiltInemantic(spv::BuiltIn builtin)
        {
            std::string semantic;
            switch (builtin)
            {
            case spv::BuiltInPosition:
            case spv::BuiltInFragCoord:
                semantic = "SV_Position";
                break;

            case spv::BuiltInFragDepth:
                semantic = "SV_Depth";
                break;

            case spv::BuiltInVertexId:
            case spv::BuiltInVertexIndex:
                semantic = "SV_VertexID";
                break;

            case spv::BuiltInInstanceId:
            case spv::BuiltInInstanceIndex:
                semantic = "SV_InstanceID";
                break;

            case spv::BuiltInSampleId:
                semantic = "SV_SampleIndex";
                break;

            case spv::BuiltInSampleMask:
                semantic = "SV_Coverage";
                break;

            case spv::BuiltInTessLevelInner:
                semantic = "SV_InsideTessFactor";
                break;

            case spv::BuiltInTessLevelOuter:
                semantic = "SV_TessFactor";
                break;

            case spv::BuiltInGlobalInvocationId:
            case spv::BuiltInLocalInvocationId:
            case spv::BuiltInLocalInvocationIndex:
            case spv::BuiltInWorkgroupId:
            case spv::BuiltInFrontFacing:
            case spv::BuiltInInvocationId:
            case spv::BuiltInPrimitiveId:
            case spv::BuiltInTessCoord:
                break;

            default:
                llvm_unreachable("Unsupported builtin.");
            }
            return semantic;
        }

        static void ExtractReflection(ResourceDesc& reflectionDesc, const spirv_cross::Compiler& compiler, spirv_cross::ID id)
        {
            const uint32_t descSet = compiler.get_decoration(id, spv::DecorationDescriptorSet);
            const uint32_t binding = compiler.get_decoration(id, spv::DecorationBinding);

            const std::string& res_name = compiler.get_name(id);
            std::strncpy(reflectionDesc.name, res_name.c_str(), sizeof(reflectionDesc.name));
            reflectionDesc.space = descSet;
            reflectionDesc.bindPoint = binding;
            reflectionDesc.bindCount = 1;
        }

        static void ExtractParameter(SignatureParameterDesc& paramDesc, const spirv_cross::Compiler& compiler,
                                     const spirv_cross::Resource& resource, const std::string& semantic)
        {
            paramDesc.semanticIndex = 0;
            for (auto iter = semantic.rbegin(); iter != semantic.rend(); ++iter)
            {
                if (!std::isdigit(*iter))
                {
                    const int sep = static_cast<int>(std::distance(semantic.begin(), iter.base()));
                    const std::string indexPart = semantic.substr(sep);
                    if (indexPart.empty())
                    {
                        paramDesc.semanticIndex = 0;
                    }
                    else
                    {
                        paramDesc.semanticIndex = std::atoi(indexPart.c_str());
                    }
                    std::strncpy(paramDesc.semantic, semantic.c_str(), std::min<int>(sep, sizeof(paramDesc.semantic)));
                    break;
                }
            }

            const auto& type = compiler.get_type(resource.type_id);
            switch (type.basetype)
            {
            case spirv_cross::SPIRType::UInt:
                paramDesc.componentType = VariableType::DataType::Uint;
                break;
            case spirv_cross::SPIRType::Int:
                paramDesc.componentType = VariableType::DataType::Int;
                break;
            case spirv_cross::SPIRType::Float:
                paramDesc.componentType = VariableType::DataType::Float;
                break;

            default:
                llvm_unreachable("Unsupported parameter component type.");
                break;
            }

            if (type.vecsize > 0)
            {
                paramDesc.mask = ComponentMask::X;
            }
            if (type.vecsize > 1)
            {
                paramDesc.mask |= ComponentMask::Y;
            }
            if (type.vecsize > 2)
            {
                paramDesc.mask |= ComponentMask::Z;
            }
            if (type.vecsize > 3)
            {
                paramDesc.mask |= ComponentMask::W;
            }

            paramDesc.location = compiler.get_decoration(resource.id, spv::DecorationLocation);
        }

        static void ExtractParameter(SignatureParameterDesc& paramDesc, const spirv_cross::Compiler& compiler,
                                     const spirv_cross::Resource& resource)
        {
            ExtractParameter(paramDesc, compiler, resource, compiler.get_name(resource.id));
        }

    private:
        std::vector<ResourceDesc> m_resourceDescs;
        std::vector<ConstantBuffer> m_constantBuffers;

        std::vector<SignatureParameterDesc> m_inputParams;
        std::vector<SignatureParameterDesc> m_outputParams;

        PrimitiveTopology m_gsHSInputPrimitive;
        PrimitiveTopology m_gsOutputTopology;
        uint32_t m_gsMaxNumOutputVertices;
        uint32_t m_gsNumInstances;

        TessellatorOutputPrimitive m_hsOutputPrimitive;
        TessellatorPartitioning m_hsPartitioning;
        TessellatorDomain m_hSDSTessellatorDomain;
        std::vector<SignatureParameterDesc> m_hsDSPatchConstantParams;
        uint32_t m_hsDSNumCtrlPoints;

        uint32_t m_csBlockSizeX;
        uint32_t m_csBlockSizeY;
        uint32_t m_csBlockSizeZ;
    };

    Reflection::Reflection() noexcept = default;

    Reflection::Reflection(const Reflection& other) : m_impl(other.m_impl ? new ReflectionImpl(*other.m_impl) : nullptr)
    {
    }

    Reflection::Reflection(Reflection&& other) noexcept : m_impl(std::move(other.m_impl))
    {
        other.m_impl = nullptr;
    }

    Reflection::~Reflection() noexcept
    {
        delete m_impl;
    }

    Reflection& Reflection::operator=(const Reflection& other)
    {
        if (this != &other)
        {
            delete m_impl;
            m_impl = nullptr;

            if (other.m_impl)
            {
                m_impl = new ReflectionImpl(*other.m_impl);
            }
        }
        return *this;
    }

    Reflection& Reflection::operator=(Reflection&& other) noexcept
    {
        if (this != &other)
        {
            delete m_impl;
            m_impl = std::move(other.m_impl);
            other.m_impl = nullptr;
        }
        return *this;
    }

    bool Reflection::Valid() const noexcept
    {
        return m_impl != nullptr;
    }

    uint32_t Reflection::NumResources() const noexcept
    {
        return m_impl->NumResources();
    }

    const Reflection::ResourceDesc* Reflection::ResourceByIndex(uint32_t index) const noexcept
    {
        return m_impl->ResourceByIndex(index);
    }

    const Reflection::ResourceDesc* Reflection::ResourceByName(const char* name) const noexcept
    {
        return m_impl->ResourceByName(name);
    }

    uint32_t Reflection::NumConstantBuffers() const noexcept
    {
        return m_impl->NumConstantBuffers();
    }

    const Reflection::ConstantBuffer* Reflection::ConstantBufferByIndex(uint32_t index) const noexcept
    {
        return m_impl->ConstantBufferByIndex(index);
    }

    const Reflection::ConstantBuffer* Reflection::ConstantBufferByName(const char* name) const noexcept
    {
        return m_impl->ConstantBufferByName(name);
    }

    uint32_t Reflection::NumInputParameters() const noexcept
    {
        return m_impl->NumInputParameters();
    }

    const Reflection::SignatureParameterDesc* Reflection::InputParameter(uint32_t index) const noexcept
    {
        return m_impl->InputParameter(index);
    }

    uint32_t Reflection::NumOutputParameters() const noexcept
    {
        return m_impl->NumOutputParameters();
    }

    const Reflection::SignatureParameterDesc* Reflection::OutputParameter(uint32_t index) const noexcept
    {
        return m_impl->OutputParameter(index);
    }

    Reflection::PrimitiveTopology Reflection::GSHSInputPrimitive() const noexcept
    {
        return m_impl->GSHSInputPrimitive();
    }

    Reflection::PrimitiveTopology Reflection::GSOutputTopology() const noexcept
    {
        return m_impl->GSOutputTopology();
    }

    uint32_t Reflection::GSMaxNumOutputVertices() const noexcept
    {
        return m_impl->GSMaxNumOutputVertices();
    }

    uint32_t Reflection::GSNumInstances() const noexcept
    {
        return m_impl->GSNumInstances();
    }

    Reflection::TessellatorOutputPrimitive Reflection::HSOutputPrimitive() const noexcept
    {
        return m_impl->HSOutputPrimitive();
    }

    Reflection::TessellatorPartitioning Reflection::HSPartitioning() const noexcept
    {
        return m_impl->HSPartitioning();
    }

    Reflection::TessellatorDomain Reflection::HSDSTessellatorDomain() const noexcept
    {
        return m_impl->HSDSTessellatorDomain();
    }

    uint32_t Reflection::HSDSNumPatchConstantParameters() const noexcept
    {
        return m_impl->HSDSNumPatchConstantParameters();
    }

    const Reflection::SignatureParameterDesc* Reflection::HSDSPatchConstantParameter(uint32_t index) const noexcept
    {
        return m_impl->HSDSPatchConstantParameter(index);
    }

    uint32_t Reflection::HSDSNumConrolPoints() const noexcept
    {
        return m_impl->HSDSNumConrolPoints();
    }

    uint32_t Reflection::CSBlockSizeX() const noexcept
    {
        return m_impl->CSBlockSizeX();
    }

    uint32_t Reflection::CSBlockSizeY() const noexcept
    {
        return m_impl->CSBlockSizeY();
    }

    uint32_t Reflection::CSBlockSizeZ() const noexcept
    {
        return m_impl->CSBlockSizeZ();
    }


    Compiler::ResultDesc Compiler::Compile(const SourceDesc& source, const Options& options, const TargetDesc& target)
    {
        ResultDesc result;
        Compiler::Compile(source, options, &target, 1, &result);
        return result;
    }

    void Compiler::Compile(const SourceDesc& source, const Options& options, const TargetDesc* targets, uint32_t numTargets,
                           ResultDesc* results)
    {
        SourceDesc sourceOverride = source;
        if (!sourceOverride.entryPoint || (std::strlen(sourceOverride.entryPoint) == 0))
        {
            sourceOverride.entryPoint = "main";
        }
        if (!sourceOverride.loadIncludeCallback)
        {
            sourceOverride.loadIncludeCallback = DefaultLoadCallback;
        }

        bool hasDxil = false;
        bool hasDxilModule = false;
        bool hasSpirV = false;
        for (uint32_t i = 0; i < numTargets; ++i)
        {
            if (targets[i].language == ShadingLanguage::Dxil)
            {
                hasDxil = true;
                if (targets[i].asModule)
                {
                    hasDxilModule = true;
                }
            }
            else
            {
                hasSpirV = true;
            }
        }

        ResultDesc dxilBinaryResult{};
        if (hasDxil)
        {
            dxilBinaryResult = CompileToBinary(sourceOverride, options, ShadingLanguage::Dxil, false);
        }

        ResultDesc dxilModuleBinaryResult{};
        if (hasDxilModule)
        {
            dxilModuleBinaryResult = CompileToBinary(sourceOverride, options, ShadingLanguage::Dxil, true);
        }

        ResultDesc spirvBinaryResult{};
        if (hasSpirV)
        {
            spirvBinaryResult = CompileToBinary(sourceOverride, options, ShadingLanguage::SpirV, false);
        }

        for (uint32_t i = 0; i < numTargets; ++i)
        {
            ResultDesc binaryResult;
            if (targets[i].language == ShadingLanguage::Dxil)
            {
                if (targets[i].asModule)
                {
                    binaryResult = dxilModuleBinaryResult;
                }
                else
                {
                    binaryResult = dxilBinaryResult;
                }
            }
            else
            {
                binaryResult = spirvBinaryResult;
            }

            results[i] = ConvertBinary(binaryResult, sourceOverride, options, targets[i]);
        }
    }

    Compiler::ResultDesc Compiler::Disassemble(const DisassembleDesc& source)
    {
        assert((source.language == ShadingLanguage::SpirV) || (source.language == ShadingLanguage::Dxil));

        Compiler::ResultDesc ret;

        ret.isText = true;

        if (source.language == ShadingLanguage::SpirV)
        {
            const uint32_t* spirvIr = reinterpret_cast<const uint32_t*>(source.binary);
            const size_t spirvSize = source.binarySize / sizeof(uint32_t);

            spv_context context = spvContextCreate(SPV_ENV_UNIVERSAL_1_3);
            uint32_t options = SPV_BINARY_TO_TEXT_OPTION_NONE | SPV_BINARY_TO_TEXT_OPTION_INDENT | SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES;
            spv_text text = nullptr;
            spv_diagnostic diagnostic = nullptr;

            spv_result_t error = spvBinaryToText(context, spirvIr, spirvSize, options, &text, &diagnostic);
            spvContextDestroy(context);

            if (error)
            {
                ret.errorWarningMsg.Reset(diagnostic->error, static_cast<uint32_t>(std::strlen(diagnostic->error)));
                ret.hasError = true;
                spvDiagnosticDestroy(diagnostic);
            }
            else
            {
                const std::string disassemble = text->str;
                ret.target.Reset(disassemble.data(), static_cast<uint32_t>(disassemble.size()));
                ret.hasError = false;
            }

            spvTextDestroy(text);
        }
        else
        {
            CComPtr<IDxcBlobEncoding> blob;
            CComPtr<IDxcBlobEncoding> disassembly;
            IFT(Dxcompiler::Instance().Library()->CreateBlobWithEncodingOnHeapCopy(source.binary, source.binarySize, CP_UTF8, &blob));
            IFT(Dxcompiler::Instance().Compiler()->Disassemble(blob, &disassembly));

            if (disassembly != nullptr)
            {
                // Remove the tailing \0
                ret.target.Reset(disassembly->GetBufferPointer(), static_cast<uint32_t>(disassembly->GetBufferSize() - 1));
                ret.hasError = false;
            }
            else
            {
                ret.hasError = true;
            }
        }

        return ret;
    }

    bool Compiler::LinkSupport()
    {
        return Dxcompiler::Instance().LinkerSupport();
    }

    Compiler::ResultDesc Compiler::Link(const LinkDesc& modules, const Compiler::Options& options, const TargetDesc& target)
    {
        auto linker = Dxcompiler::Instance().CreateLinker();
        IFTPTR(linker);

        auto* library = Dxcompiler::Instance().Library();

        std::vector<std::wstring> moduleNames(modules.numModules);
        std::vector<const wchar_t*> moduleNamesUtf16(modules.numModules);
        std::vector<CComPtr<IDxcBlobEncoding>> moduleBlobs(modules.numModules);
        for (uint32_t i = 0; i < modules.numModules; ++i)
        {
            IFTARG(modules.modules[i] != nullptr);

            IFT(library->CreateBlobWithEncodingOnHeapCopy(modules.modules[i]->target.Data(), modules.modules[i]->target.Size(), CP_UTF8,
                                                          &moduleBlobs[i]));
            IFTARG(moduleBlobs[i]->GetBufferSize() >= 4);

            Unicode::UTF8ToUTF16String(modules.modules[i]->name, &moduleNames[i]);
            moduleNamesUtf16[i] = moduleNames[i].c_str();
            IFT(linker->RegisterLibrary(moduleNamesUtf16[i], moduleBlobs[i]));
        }

        std::wstring entryPointUtf16;
        Unicode::UTF8ToUTF16String(modules.entryPoint, &entryPointUtf16);

        const std::wstring shaderProfile = ShaderProfileName(modules.stage, options.shaderModel);
        CComPtr<IDxcOperationResult> linkResult;
        IFT(linker->Link(entryPointUtf16.c_str(), shaderProfile.c_str(), moduleNamesUtf16.data(),
                         static_cast<UINT32>(moduleNamesUtf16.size()), nullptr, 0, &linkResult));

        Compiler::ResultDesc binaryResult{};
        ConvertDxcResult(binaryResult, linkResult, ShadingLanguage::Dxil, false, false);

        Compiler::SourceDesc source{};
        source.entryPoint = modules.entryPoint;
        source.stage = modules.stage;
        return ConvertBinary(binaryResult, source, options, target);
    }
} // namespace ShaderConductor

namespace
{
#ifdef LLVM_ON_WIN32
    Reflection MakeDxilReflection(IDxcBlob* dxilBlob)
    {
        return Reflection::ReflectionImpl::Make(dxilBlob);
    }
#endif

    Reflection MakeSpirVReflection(const spirv_cross::Compiler& compiler)
    {
        return Reflection::ReflectionImpl::Make(compiler);
    }
} // namespace

#ifdef _WIN32
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    SC_UNUSED(instance);

    BOOL result = TRUE;
    if (reason == DLL_PROCESS_DETACH)
    {
        dllDetaching = true;

        if (reserved == 0)
        {
            // FreeLibrary has been called or the DLL load failed
            Dxcompiler::Instance().Destroy();
        }
        else
        {
            // Process termination. We should not call FreeLibrary()
            Dxcompiler::Instance().Terminate();
        }
    }

    return result;
}
#endif
