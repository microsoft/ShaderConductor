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
#include <memory>

#include <dxc/DxilContainer/DxilContainer.h>
#include <dxc/dxcapi.h>
#include <llvm/Support/ErrorHandling.h>

#include <spirv-tools/libspirv.h>
#include <spirv.hpp>
#include <spirv_cross.hpp>
#include <spirv_glsl.hpp>
#include <spirv_hlsl.hpp>
#include <spirv_msl.hpp>
#include <spirv_cross_util.hpp>

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

    void ShaderReflection(Compiler::ReflectionResultDesc& result, IDxcBlob* dxilBlob)
    {
        CComPtr<ID3D12ShaderReflection> shaderReflection;
        IFT(CreateDxcReflectionFromBlob(dxilBlob, shaderReflection));

        D3D12_SHADER_DESC shaderDesc;
        shaderReflection->GetDesc(&shaderDesc);

        std::vector<Compiler::ReflectionDesc> vecReflectionDescs;
        for (uint32_t resourceIndex = 0; resourceIndex < shaderDesc.BoundResources; ++resourceIndex)
        {
            D3D12_SHADER_INPUT_BIND_DESC bindDesc;
            shaderReflection->GetResourceBindingDesc(resourceIndex, &bindDesc);

            Compiler::ReflectionDesc reflectionDesc{};

            if (bindDesc.Type == D3D_SIT_CBUFFER || bindDesc.Type == D3D_SIT_TBUFFER)
            {
                ID3D12ShaderReflectionConstantBuffer* constantBuffer = shaderReflection->GetConstantBufferByName(bindDesc.Name);

                D3D12_SHADER_BUFFER_DESC bufferDesc;
                constantBuffer->GetDesc(&bufferDesc);

                if (strcmp(bufferDesc.Name, "$Globals") == 0)
                {
                    for (uint32_t variableIndex = 0; variableIndex < bufferDesc.Variables; ++variableIndex)
                    {
                        ID3D12ShaderReflectionVariable* variable = constantBuffer->GetVariableByIndex(variableIndex);
                        D3D12_SHADER_VARIABLE_DESC variableDesc;
                        variable->GetDesc(&variableDesc);

                        std::strncpy(reflectionDesc.name, variableDesc.Name,
                                     std::min(std::strlen(variableDesc.Name) + 1, sizeof(reflectionDesc.name)));

                        reflectionDesc.type = ShaderResourceType::Parameter;
                        reflectionDesc.bufferBindPoint = bindDesc.BindPoint;
                        reflectionDesc.bindPoint = variableDesc.StartOffset;
                        reflectionDesc.bindCount = variableDesc.Size;
                    }
                }
                else
                {
                    std::strncpy(reflectionDesc.name, bufferDesc.Name,
                                 std::min(std::strlen(bufferDesc.Name) + 1, sizeof(reflectionDesc.name)));

                    reflectionDesc.type = ShaderResourceType::ConstantBuffer;
                    reflectionDesc.bufferBindPoint = bindDesc.BindPoint;
                    reflectionDesc.bindPoint = 0;
                    reflectionDesc.bindCount = 0;
                }
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

                std::strncpy(reflectionDesc.name, bindDesc.Name, std::min(std::strlen(bindDesc.Name) + 1, sizeof(reflectionDesc.name)));

                reflectionDesc.bufferBindPoint = 0;
                reflectionDesc.bindPoint = bindDesc.BindPoint;
                reflectionDesc.bindCount = bindDesc.BindCount;
            }

            vecReflectionDescs.push_back(reflectionDesc);
        }

        result.descCount = static_cast<uint32_t>(vecReflectionDescs.size());
        result.descs.Reset(vecReflectionDescs.data(), sizeof(Compiler::ReflectionDesc) * result.descCount);
        result.instructionCount = shaderDesc.InstructionCount;
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

    void ConvertDxcResult(Compiler::ResultDesc& result, IDxcOperationResult* dxcResult, ShadingLanguage targetLanguage, bool asModule)
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

#ifdef LLVM_ON_WIN32
            if ((targetLanguage == ShadingLanguage::Dxil) && !asModule)
            {
                // Gather reflection information only for ShadingLanguage::Dxil
                ShaderReflection(result.reflection, program);
            }
#else
            SC_UNUSED(targetLanguage);
            SC_UNUSED(asModule);
#endif
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
        ConvertDxcResult(ret, compileResult, targetLanguage, asModule);

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
            ret.reflection.descs.Reset(binaryResult.reflection.descs.Data(),
                                       sizeof(Compiler::ReflectionDesc) * binaryResult.reflection.descCount);
            ret.reflection.descCount = binaryResult.reflection.descCount;
            ret.reflection.instructionCount = binaryResult.reflection.instructionCount;
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
        ConvertDxcResult(binaryResult, linkResult, ShadingLanguage::Dxil, false);

        Compiler::SourceDesc source{};
        source.entryPoint = modules.entryPoint;
        source.stage = modules.stage;
        return ConvertBinary(binaryResult, source, options, target);
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
