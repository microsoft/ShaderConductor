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
#include <fstream>

#include <dxc/dxcapi.h>
#include <llvm/Support/ErrorHandling.h>

#include <spirv-tools/libspirv.h>
#include <spirv.hpp>
#include <spirv_cross.hpp>
#include <spirv_glsl.hpp>
#include <spirv_hlsl.hpp>
#include <spirv_msl.hpp>

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

        void Destroy()
        {
            if (m_dxcompilerDll)
            {
                m_compiler = nullptr;
                m_library = nullptr;

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
        }

    private:
        HMODULE m_dxcompilerDll = nullptr;
        DxcCreateInstanceProc m_createInstanceFunc = nullptr;

        CComPtr<IDxcLibrary> m_library;
        CComPtr<IDxcCompiler> m_compiler;
    };

    class ScIncludeHandler : public IDxcIncludeHandler
    {
    public:
        explicit ScIncludeHandler(std::function<std::string(const std::string& includeName)> loadCallback)
            : m_loadCallback(std::move(loadCallback))
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

            const std::string source = m_loadCallback(utf8FileName);
            if (source.empty())
            {
                return E_FAIL;
            }

            *includeSource = nullptr;
            return Dxcompiler::Instance().Library()->CreateBlobWithEncodingOnHeapCopy(
                source.data(), static_cast<UINT32>(source.size()), CP_UTF8, reinterpret_cast<IDxcBlobEncoding**>(includeSource));
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
        std::function<std::string(const std::string& includeName)> m_loadCallback;

        std::atomic<ULONG> m_ref = 0;
    };

    std::string DefaultLoadCallback(const std::string& includeName)
    {
        std::vector<char> ret;
        std::ifstream includeFile(includeName, std::ios_base::in);
        includeFile.seekg(0, std::ios::end);
        ret.resize(includeFile.tellg());
        includeFile.seekg(0, std::ios::beg);
        includeFile.read(ret.data(), ret.size());
        while (!ret.empty() && (ret.back() == '\0'))
        {
            ret.pop_back();
        }
        return std::string(ret.data(), ret.size());
    }

    Compiler::ResultDesc AppendError(const Compiler::ResultDesc& previousResult, const std::string& msg)
    {
        Compiler::ResultDesc ret;
        ret.isText = previousResult.isText;
        ret.errorWarningMsg = previousResult.errorWarningMsg;
        if (!ret.errorWarningMsg.empty())
        {
            ret.errorWarningMsg += "\n";
        }
        ret.errorWarningMsg += msg;
        ret.hasError = true;

        return ret;
    }

    Compiler::ResultDesc CompileToBinary(const Compiler::SourceDesc& source, ShadingLanguage targetLanguage)
    {
        assert((targetLanguage == ShadingLanguage::Dxil) || (targetLanguage == ShadingLanguage::SpirV));

        std::wstring shaderProfile;
        switch (source.stage)
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
        shaderProfile += L"_6_0";

        std::vector<DxcDefine> dxcDefines;
        std::vector<std::wstring> dxcDefineStrings;
        // Need to reserve capacity so that small-string optimization does not
        // invalidate the pointers to internal string data while resizing.
        dxcDefineStrings.reserve(source.defines.size() * 2);
        for (const auto& define : source.defines)
        {
            std::wstring nameUtf16Str;
            Unicode::UTF8ToUTF16String(define.name.c_str(), &nameUtf16Str);
            dxcDefineStrings.emplace_back(std::move(nameUtf16Str));
            const wchar_t* nameUtf16 = dxcDefineStrings.back().c_str();

            const wchar_t* valueUtf16;
            if (!define.value.empty())
            {
                std::wstring valueUtf16Str;
                Unicode::UTF8ToUTF16String(define.value.c_str(), &valueUtf16Str);
                dxcDefineStrings.emplace_back(std::move(valueUtf16Str));
                valueUtf16 = dxcDefineStrings.back().c_str();
            }
            else
            {
                valueUtf16 = nullptr;
            }

            dxcDefines.push_back({ nameUtf16, valueUtf16 });
        }

        CComPtr<IDxcBlobEncoding> sourceBlob;
        IFT(Dxcompiler::Instance().Library()->CreateBlobWithEncodingOnHeapCopy(
            source.source.data(), static_cast<UINT32>(source.source.size()), CP_UTF8, &sourceBlob));
        IFTARG(sourceBlob->GetBufferSize() >= 4);

        std::wstring shaderNameUtf16;
        Unicode::UTF8ToUTF16String(source.fileName.c_str(), &shaderNameUtf16);

        std::wstring entryPointUtf16;
        Unicode::UTF8ToUTF16String(source.entryPoint.c_str(), &entryPointUtf16);

        // clang-format off
        std::vector<std::wstring> dxcArgStrings =
        {
            L"-T", shaderProfile,
            L"-E", entryPointUtf16,
        };
        // clang-format on

        switch (targetLanguage)
        {
        case ShadingLanguage::Dxil:
            break;

        case ShadingLanguage::SpirV:
        case ShadingLanguage::Hlsl:
        case ShadingLanguage::Glsl:
        case ShadingLanguage::Essl:
        case ShadingLanguage::Msl:
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

        HRESULT status;
        IFT(compileResult->GetStatus(&status));

        Compiler::ResultDesc ret;

        ret.isText = false;

        CComPtr<IDxcBlobEncoding> errors;
        IFT(compileResult->GetErrorBuffer(&errors));
        if (errors != nullptr)
        {
            ret.errorWarningMsg.assign(reinterpret_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
            errors = nullptr;
        }

        ret.hasError = true;
        if (SUCCEEDED(status))
        {
            CComPtr<IDxcBlob> program;
            IFT(compileResult->GetResult(&program));
            compileResult = nullptr;
            if (program != nullptr)
            {
                const uint8_t* programPtr = reinterpret_cast<const uint8_t*>(program->GetBufferPointer());
                ret.target.assign(programPtr, programPtr + program->GetBufferSize());

                ret.hasError = false;
            }
        }

        return ret;
    }

    Compiler::ResultDesc ConvertBinary(const Compiler::ResultDesc& binaryResult, const Compiler::SourceDesc& source,
                                       const Compiler::TargetDesc& target)
    {
        assert((target.language != ShadingLanguage::Dxil) && (target.language != ShadingLanguage::SpirV));
        assert((binaryResult.target.size() & (sizeof(uint32_t) - 1)) == 0);

        Compiler::ResultDesc ret;

        ret.errorWarningMsg = binaryResult.errorWarningMsg;
        ret.isText = true;

        uint32_t intVersion = 0;
        if (!target.version.empty())
        {
            intVersion = std::stoi(target.version);
        }

        const uint32_t* spirvIr = reinterpret_cast<const uint32_t*>(binaryResult.target.data());
        const size_t spirvSize = binaryResult.target.size() / sizeof(uint32_t);

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
                return AppendError(ret, "GS, HS, and DS has not been supported yet.");
            }
            if ((source.stage == ShaderStage::GeometryShader) && (intVersion < 40))
            {
                return AppendError(ret, "HLSL shader model earlier than 4.0 doesn't have GS or CS.");
            }
            if ((source.stage == ShaderStage::ComputeShader) && (intVersion < 50))
            {
                return AppendError(ret, "CS in HLSL shader model earlier than 5.0 is not supported.");
            }
            if (((source.stage == ShaderStage::HullShader) || (source.stage == ShaderStage::DomainShader)) && (intVersion < 50))
            {
                return AppendError(ret, "HLSL shader model earlier than 5.0 doesn't have HS or DS.");
            }
            compiler = std::make_unique<spirv_cross::CompilerHLSL>(spirvIr, spirvSize);
            break;

        case ShadingLanguage::Glsl:
        case ShadingLanguage::Essl:
            compiler = std::make_unique<spirv_cross::CompilerGLSL>(spirvIr, spirvSize);
            combinedImageSamplers = true;
            buildDummySampler = true;
            break;

        case ShadingLanguage::Msl:
            if (source.stage == ShaderStage::GeometryShader)
            {
                return AppendError(ret, "MSL doesn't have GS.");
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
        if (!target.version.empty())
        {
            opts.version = intVersion;
        }
        opts.es = (target.language == ShadingLanguage::Essl);
        opts.force_temporary = false;
        opts.separate_shader_objects = true;
        opts.flatten_multidimensional_arrays = false;
        opts.enable_420pack_extension = (target.language == ShadingLanguage::Glsl) && (target.version.empty() || (opts.version >= 420));
        opts.vulkan_semantics = false;
        opts.vertex.fixup_clipspace = false;
        opts.vertex.flip_vert_y = false;
        opts.vertex.support_nonzero_base_instance = true;
        compiler->set_common_options(opts);

        if (target.language == ShadingLanguage::Hlsl)
        {
            auto* hlslCompiler = static_cast<spirv_cross::CompilerHLSL*>(compiler.get());
            auto hlslOpts = hlslCompiler->get_hlsl_options();
            if (!target.version.empty())
            {
                if (opts.version < 30)
                {
                    return AppendError(ret, "HLSL shader model earlier than 3.0 is not supported.");
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
        else if (target.language == ShadingLanguage::Msl)
        {
            auto* mslCompiler = static_cast<spirv_cross::CompilerMSL*>(compiler.get());
            auto mslOpts = mslCompiler->get_msl_options();
            if (!target.version.empty())
            {
                mslOpts.msl_version = opts.version;
            }
            mslOpts.swizzle_texture_samples = false;
            mslCompiler->set_msl_options(mslOpts);
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
            ret.target.assign(targetStr.begin(), targetStr.end());
            ret.hasError = false;
        }
        catch (spirv_cross::CompilerError& error)
        {
            ret.errorWarningMsg = error.what();
            ret.hasError = true;
        }

        return ret;
    }
} // namespace

namespace ShaderConductor
{
    Compiler::ResultDesc Compiler::Compile(SourceDesc source, TargetDesc target)
    {
        if (source.entryPoint.empty())
        {
            source.entryPoint = "main";
        }
        if (!source.loadIncludeCallback)
        {
            source.loadIncludeCallback = DefaultLoadCallback;
        }

        const auto binaryLanguage = target.language == ShadingLanguage::Dxil ? ShadingLanguage::Dxil : ShadingLanguage::SpirV;
        auto ret = CompileToBinary(source, binaryLanguage);

        if (!ret.hasError && (target.language != binaryLanguage))
        {
            ret = ConvertBinary(ret, source, target);
        }

        return ret;
    }

    Compiler::ResultDesc Compiler::Disassemble(DisassembleDesc source)
    {
        assert(source.language == ShadingLanguage::SpirV);

        Compiler::ResultDesc ret;
        ret.isText = true;

        const uint32_t* spirvIr = reinterpret_cast<const uint32_t*>(source.binary.data());
        const size_t spirvSize = source.binary.size() / sizeof(uint32_t);

        spv_context context = spvContextCreate(SPV_ENV_UNIVERSAL_1_3);
        uint32_t options = SPV_BINARY_TO_TEXT_OPTION_NONE | SPV_BINARY_TO_TEXT_OPTION_INDENT | SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES;
        spv_text text = nullptr;
        spv_diagnostic diagnostic = nullptr;

        spv_result_t error = spvBinaryToText(context, spirvIr, spirvSize, options, &text, &diagnostic);
        spvContextDestroy(context);

        if (error)
        {
            ret.errorWarningMsg = diagnostic->error;
            ret.hasError = true;
            spvDiagnosticDestroy(diagnostic);
        }
        else
        {
            std::string disassemble = std::string(text->str);           
            ret.target.assign(disassemble.begin(), disassemble.end());
            ret.hasError = false;
        }

        spvTextDestroy(text);
        return ret;
    }
} // namespace ShaderConductor

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
