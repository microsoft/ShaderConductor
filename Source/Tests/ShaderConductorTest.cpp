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

#include <gtest/gtest.h>

#include <cassert>
#include <fstream>
#include <string>
#include <tuple>
#include <vector>

using namespace ShaderConductor;

namespace
{
    std::vector<uint8_t> LoadFile(const std::string& name, bool isText)
    {
        std::vector<uint8_t> ret;
        std::ios_base::openmode mode = std::ios_base::in;
        if (!isText)
        {
            mode |= std::ios_base::binary;
        }
        std::ifstream file(name, mode);
        if (file)
        {
            file.seekg(0, std::ios::end);
            ret.resize(static_cast<size_t>(file.tellg()));
            file.seekg(0, std::ios::beg);
            file.read(reinterpret_cast<char*>(ret.data()), ret.size());
            ret.resize(static_cast<size_t>(file.gcount()));
        }
        return ret;
    }

    void CompareWithExpected(const std::vector<uint8_t>& actual, bool isText, const std::string& compareName)
    {
        std::vector<uint8_t> expected = LoadFile(TEST_DATA_DIR "Expected/" + compareName, isText);
        if (expected != actual)
        {
            if (!actual.empty())
            {
                std::ios_base::openmode mode = std::ios_base::out;
                if (!isText)
                {
                    mode |= std::ios_base::binary;
                }
                std::ofstream actualFile(TEST_DATA_DIR "Result/" + compareName, mode);
                actualFile.write(reinterpret_cast<const char*>(actual.data()), actual.size());
            }
        }

        EXPECT_EQ(std::string(expected.begin(), expected.end()), std::string(actual.begin(), actual.end()));
    }

    void HlslToAnyTest(const std::string& name, const Compiler::SourceDesc& source, const Compiler::Options& options,
                       const std::vector<Compiler::TargetDesc>& targets, const std::vector<bool>& expectSuccessFlags)
    {
        static const std::string extMap[] = {"dxil", "spv", "hlsl", "glsl", "essl", "msl", "msl"};
        static_assert(sizeof(extMap) / sizeof(extMap[0]) == static_cast<uint32_t>(ShadingLanguage::NumShadingLanguages),
                      "extMap doesn't match with the number of shading languages.");

        std::vector<Compiler::ResultDesc> results(targets.size());
        Compiler::Compile(source, options, targets.data(), static_cast<uint32_t>(targets.size()), results.data());
        for (size_t i = 0; i < targets.size(); ++i)
        {
            const auto& result = results[i];
            if (expectSuccessFlags[i])
            {
                EXPECT_FALSE(result.hasError);
                EXPECT_EQ(result.errorWarningMsg, nullptr);
                EXPECT_TRUE(result.isText);

                std::string compareName = name;
                if (targets[i].version != nullptr)
                {
                    compareName += "." + std::string(targets[i].version);
                }
                compareName += "." + extMap[static_cast<uint32_t>(targets[i].language)];

                const uint8_t* target_ptr = reinterpret_cast<const uint8_t*>(result.target->Data());
                CompareWithExpected(std::vector<uint8_t>(target_ptr, target_ptr + result.target->Size()), result.isText, compareName);
            }
            else
            {
                EXPECT_TRUE(result.hasError);
                EXPECT_EQ(result.target, nullptr);
            }

            DestroyBlob(result.errorWarningMsg);
            DestroyBlob(result.target);
            Compiler::DestroyResultDesc(result);
        }
    }

    Compiler::ModuleDesc CompileToModule(const char* moduleName, const std::string& inputFileName, const Compiler::TargetDesc& target)
    {
        std::vector<uint8_t> input = LoadFile(inputFileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        const auto result = Compiler::Compile({source.c_str(), inputFileName.c_str(), "", ShaderStage::PixelShader}, {}, target);

        EXPECT_FALSE(result.hasError);
        EXPECT_FALSE(result.isText);

        DestroyBlob(result.errorWarningMsg);
        Compiler::DestroyResultDesc(result);

        return {moduleName, result.target};
    }

    class TestBase : public testing::Test
    {
    public:
        TestBase()
        {
            m_expectSuccessFlags.assign(m_testTargets.size(), true);
        }

        void SetUp() override
        {
            for (auto& src : m_testSources)
            {
                const std::string& name = std::get<0>(src);
                Compiler::SourceDesc& source = std::get<1>(src);

                std::get<2>(src) = TEST_DATA_DIR "Input/" + name + ".hlsl";
                source.fileName = std::get<2>(src).c_str();

                std::vector<uint8_t> input = LoadFile(source.fileName, true);
                std::get<3>(src) = std::string(reinterpret_cast<char*>(input.data()), input.size());
                source.source = std::get<3>(src).c_str();
            }
        }

        void RunTests(ShadingLanguage targetSl, const Compiler::Options& options = {})
        {
            for (const auto& combination : m_testSources)
            {
                std::vector<Compiler::TargetDesc> targetSubset;
                std::vector<bool> expectSuccessSubset;
                for (size_t i = 0; i < m_testTargets.size(); ++i)
                {
                    if (m_testTargets[i].language == targetSl)
                    {
                        targetSubset.push_back(m_testTargets[i]);
                        expectSuccessSubset.push_back(m_expectSuccessFlags[i]);
                    }
                }

                HlslToAnyTest(std::get<0>(combination), std::get<1>(combination), options, targetSubset, expectSuccessSubset);
            }
        }

    protected:
        // test name, source desc, input file name, input source
        std::vector<std::tuple<std::string, Compiler::SourceDesc, std::string, std::string>> m_testSources;

        // clang-format off
        const std::vector<Compiler::TargetDesc> m_testTargets =
        {
            { ShadingLanguage::Hlsl, "30" },
            { ShadingLanguage::Hlsl, "40" },
            { ShadingLanguage::Hlsl, "50" },

            { ShadingLanguage::Glsl, "300" },
            { ShadingLanguage::Glsl, "410" },

            { ShadingLanguage::Essl, "300" },
            { ShadingLanguage::Essl, "310" },

            { ShadingLanguage::Msl_macOS },
        };
        // clang-format on

        std::vector<bool> m_expectSuccessFlags;
    };

    class VertexShaderTest : public TestBase
    {
    public:
        void SetUp() override
        {
            // clang-format off
            m_testSources =
            {
                {
                    "Constant_VS",
                    { "", "", "VSMain", ShaderStage::VertexShader },
                    "",
                    ""
                },
                {
                    "PassThrough_VS",
                    { "", "", "VSMain", ShaderStage::VertexShader },
                    "",
                    ""
                },
                {
                    "Transform_VS",
                    { "", "", "", ShaderStage::VertexShader },
                    "",
                    ""
                },
            };
            // clang-format on

            TestBase::SetUp();
        }
    };

    class PixelShaderTest : public TestBase
    {
    public:
        void SetUp() override
        {
            // clang-format off
            m_testSources =
            {
                {
                    "Constant_PS",
                    { "", "", "PSMain", ShaderStage::PixelShader },
                    "",
                    ""
                },
                {
                    "PassThrough_PS",
                    { "", "", "PSMain", ShaderStage::PixelShader },
                    "",
                    ""
                },
                {
                    "ToneMapping_PS",
                    { "", "", "", ShaderStage::PixelShader },
                    "",
                    ""
                },
            };
            // clang-format on

            TestBase::SetUp();
        }
    };

    class GeometryShaderTest : public TestBase
    {
    public:
        void SetUp() override
        {
            // clang-format off
            m_testSources =
            {
                {
                    "Particle_GS",
                    { "", "", "", ShaderStage::GeometryShader, defines_.data(), static_cast<uint32_t>(defines_.size()) },
                    "",
                    ""
                },
            };
            // clang-format on

            m_expectSuccessFlags[0] = false; // No GS in HLSL SM3
            m_expectSuccessFlags[1] = false; // GS not supported yet
            m_expectSuccessFlags[2] = false; // GS not supported yet
            m_expectSuccessFlags[7] = false; // No GS in MSL

            TestBase::SetUp();
        }

    private:
        std::vector<MacroDefine> defines_ = {{"FIXED_VERTEX_RADIUS", "5.0"}};
    };

    class HullShaderTest : public TestBase
    {
    public:
        void SetUp() override
        {
            // clang-format off
            m_testSources =
            {
                {
                    "DetailTessellation_HS",
                    { "", "", "", ShaderStage::HullShader },
                    "",
                    ""
                },
            };
            // clang-format on

            m_expectSuccessFlags[0] = false; // No HS in HLSL SM3
            m_expectSuccessFlags[1] = false; // No HS in HLSL SM4
            m_expectSuccessFlags[2] = false; // HS not supported yet

            TestBase::SetUp();
        }
    };

    class DomainShaderTest : public TestBase
    {
    public:
        void SetUp() override
        {
            // clang-format off
            m_testSources =
            {
                {
                    "PNTriangles_DS",
                    { "", "", "", ShaderStage::DomainShader },
                    "",
                    ""
                },
            };
            // clang-format on

            m_expectSuccessFlags[0] = false; // No HS in HLSL SM3
            m_expectSuccessFlags[1] = false; // No HS in HLSL SM4
            m_expectSuccessFlags[2] = false; // DS not supported yet

            TestBase::SetUp();
        }
    };

    class ComputeShaderTest : public TestBase
    {
    public:
        void SetUp() override
        {
            // clang-format off
            m_testSources =
            {
                {
                    "Fluid_CS",
                    { "", "", "", ShaderStage::ComputeShader },
                    "",
                    ""
                },
            };
            // clang-format on

            m_expectSuccessFlags[0] = false; // No CS in HLSL SM3
            m_expectSuccessFlags[1] = false; // CS in HLSL SM4 is not supported
            m_expectSuccessFlags[5] = false; // No CS in OpenGL ES 3.0

            TestBase::SetUp();
        }
    };


    TEST_F(VertexShaderTest, ToHlsl)
    {
        RunTests(ShadingLanguage::Hlsl);
    }

    TEST_F(VertexShaderTest, ToGlsl)
    {
        RunTests(ShadingLanguage::Glsl);
    }

    TEST_F(VertexShaderTest, ToGlslColumnMajor)
    {
        const std::string fileName = TEST_DATA_DIR "Input/Transform_VS.hlsl";

        std::vector<uint8_t> input = LoadFile(fileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        Compiler::Options options;
        options.packMatricesInRowMajor = false;

        HlslToAnyTest("Transform_VS_ColumnMajor", {source.c_str(), fileName.c_str(), nullptr, ShaderStage::VertexShader}, options,
                      {{ShadingLanguage::Glsl, "300"}}, {true});
    }

    TEST_F(VertexShaderTest, ToEssl)
    {
        RunTests(ShadingLanguage::Essl);
    }

    TEST_F(VertexShaderTest, ToMsl)
    {
        RunTests(ShadingLanguage::Msl_macOS);
    }


    TEST_F(PixelShaderTest, ToHlsl)
    {
        RunTests(ShadingLanguage::Hlsl);
    }

    TEST_F(PixelShaderTest, ToGlsl)
    {
        RunTests(ShadingLanguage::Glsl);
    }

    TEST_F(PixelShaderTest, ToEssl)
    {
        RunTests(ShadingLanguage::Essl);
    }

    TEST_F(PixelShaderTest, ToMsl)
    {
        RunTests(ShadingLanguage::Msl_macOS);
    }


    TEST_F(GeometryShaderTest, ToHlsl)
    {
        RunTests(ShadingLanguage::Hlsl);
    }

    TEST_F(GeometryShaderTest, ToGlsl)
    {
        RunTests(ShadingLanguage::Glsl);
    }

    TEST_F(GeometryShaderTest, ToEssl)
    {
        RunTests(ShadingLanguage::Essl);
    }

    TEST_F(GeometryShaderTest, ToMsl)
    {
        RunTests(ShadingLanguage::Msl_macOS);
    }


    TEST_F(HullShaderTest, ToHlsl)
    {
        RunTests(ShadingLanguage::Hlsl);
    }

    TEST_F(HullShaderTest, ToGlsl)
    {
        RunTests(ShadingLanguage::Glsl);
    }

    TEST_F(HullShaderTest, ToEssl)
    {
        RunTests(ShadingLanguage::Essl);
    }

    TEST_F(HullShaderTest, ToMsl)
    {
        RunTests(ShadingLanguage::Msl_macOS);
    }


    TEST_F(DomainShaderTest, ToHlsl)
    {
        RunTests(ShadingLanguage::Hlsl);
    }

    TEST_F(DomainShaderTest, ToGlsl)
    {
        RunTests(ShadingLanguage::Glsl);
    }

    TEST_F(DomainShaderTest, ToEssl)
    {
        RunTests(ShadingLanguage::Essl);
    }

    TEST_F(DomainShaderTest, ToMsl)
    {
        RunTests(ShadingLanguage::Msl_macOS);
    }


    TEST_F(ComputeShaderTest, ToHlsl)
    {
        RunTests(ShadingLanguage::Hlsl);
    }

    TEST_F(ComputeShaderTest, ToGlsl)
    {
        RunTests(ShadingLanguage::Glsl);
    }

    TEST_F(ComputeShaderTest, ToEssl)
    {
        RunTests(ShadingLanguage::Essl);
    }

    TEST_F(ComputeShaderTest, ToMsl)
    {
        RunTests(ShadingLanguage::Msl_macOS);
    }

    TEST(IncludeTest, IncludeExist)
    {
        const std::string fileName = TEST_DATA_DIR "Input/IncludeExist.hlsl";

        std::vector<uint8_t> input = LoadFile(fileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        const auto result =
            Compiler::Compile({source.c_str(), fileName.c_str(), "main", ShaderStage::PixelShader}, {}, {ShadingLanguage::Glsl, "30"});

        EXPECT_FALSE(result.hasError);
        EXPECT_EQ(result.errorWarningMsg, nullptr);
        EXPECT_TRUE(result.isText);

        const uint8_t* target_ptr = reinterpret_cast<const uint8_t*>(result.target->Data());
        CompareWithExpected(std::vector<uint8_t>(target_ptr, target_ptr + result.target->Size()), result.isText, "IncludeExist.glsl");

        DestroyBlob(result.errorWarningMsg);
        DestroyBlob(result.target);
        Compiler::DestroyResultDesc(result);
    }

    TEST(IncludeTest, IncludeNotExist)
    {
        const std::string fileName = TEST_DATA_DIR "Input/IncludeNotExist.hlsl";

        std::vector<uint8_t> input = LoadFile(fileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        const auto result =
            Compiler::Compile({source.c_str(), fileName.c_str(), "main", ShaderStage::PixelShader}, {}, {ShadingLanguage::Glsl, "30"});

        EXPECT_TRUE(result.hasError);
        const char* errorStr = reinterpret_cast<const char*>(result.errorWarningMsg->Data());
        EXPECT_GE(std::string(errorStr, errorStr + result.errorWarningMsg->Size()).find("fatal error: 'Header.hlsli' file not found"), 0U);

        DestroyBlob(result.errorWarningMsg);
        DestroyBlob(result.target);
        Compiler::DestroyResultDesc(result);
    }

    TEST(IncludeTest, IncludeEmptyFile)
    {
        const std::string fileName = TEST_DATA_DIR "Input/IncludeEmptyHeader.hlsl";

        std::vector<uint8_t> input = LoadFile(fileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        const auto result =
            Compiler::Compile({source.c_str(), fileName.c_str(), "main", ShaderStage::PixelShader}, {}, {ShadingLanguage::Glsl, "30"});

        EXPECT_FALSE(result.hasError);
        EXPECT_EQ(result.errorWarningMsg, nullptr);
        EXPECT_TRUE(result.isText);

        const uint8_t* target_ptr = reinterpret_cast<const uint8_t*>(result.target->Data());
        CompareWithExpected(std::vector<uint8_t>(target_ptr, target_ptr + result.target->Size()), result.isText, "IncludeEmptyHeader.glsl");

        DestroyBlob(result.errorWarningMsg);
        DestroyBlob(result.target);
        Compiler::DestroyResultDesc(result);
    }

    TEST(HalfDataTypeTest, DotHalf)
    {
        const std::string fileName = TEST_DATA_DIR "Input/HalfDataType.hlsl";

        std::vector<uint8_t> input = LoadFile(fileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        Compiler::Options option;
        option.shaderModel = {6, 2};
        option.enable16bitTypes = true;

        const auto result = Compiler::Compile({source.c_str(), fileName.c_str(), "DotHalfPS", ShaderStage::PixelShader}, option,
                                              {ShadingLanguage::Glsl, "30"});

        EXPECT_FALSE(result.hasError);
        EXPECT_TRUE(result.isText);

        const uint8_t* target_ptr = reinterpret_cast<const uint8_t*>(result.target->Data());
        CompareWithExpected(std::vector<uint8_t>(target_ptr, target_ptr + result.target->Size()), result.isText, "DotHalfPS.glsl");

        DestroyBlob(result.errorWarningMsg);
        DestroyBlob(result.target);
        Compiler::DestroyResultDesc(result);
    }

    TEST(HalfDataTypeTest, HalfOutParam)
    {
        const std::string fileName = TEST_DATA_DIR "Input/HalfDataType.hlsl";

        std::vector<uint8_t> input = LoadFile(fileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        Compiler::Options option;
        option.shaderModel = {6, 2};
        option.enable16bitTypes = true;

        const auto result = Compiler::Compile({source.c_str(), fileName.c_str(), "HalfOutParamPS", ShaderStage::PixelShader}, option,
                                              {ShadingLanguage::Glsl, "30"});

        EXPECT_FALSE(result.hasError);
        EXPECT_TRUE(result.isText);

        const uint8_t* target_ptr = reinterpret_cast<const uint8_t*>(result.target->Data());
        CompareWithExpected(std::vector<uint8_t>(target_ptr, target_ptr + result.target->Size()), result.isText, "HalfOutParamPS.glsl");

        DestroyBlob(result.errorWarningMsg);
        DestroyBlob(result.target);
        Compiler::DestroyResultDesc(result);
    }

    TEST(LinkTest, LinkDxil)
    {
        if (!Compiler::LinkSupport())
        {
            GTEST_SKIP_("Link is not supported on this platform");
        }

        const Compiler::TargetDesc target = {ShadingLanguage::Dxil, "", true};
        const Compiler::ModuleDesc dxilModules[] = {
            CompileToModule("CalcLight", TEST_DATA_DIR "Input/CalcLight.hlsl", target),
            CompileToModule("CalcLightDiffuse", TEST_DATA_DIR "Input/CalcLightDiffuse.hlsl", target),
            CompileToModule("CalcLightDiffuseSpecular", TEST_DATA_DIR "Input/CalcLightDiffuseSpecular.hlsl", target),
        };

        const Compiler::ModuleDesc* testModules[][2] = {
            {&dxilModules[0], &dxilModules[1]},
            {&dxilModules[0], &dxilModules[2]},
        };

#ifdef NDEBUG
        const std::string expectedNames[] = {"CalcLight+Diffuse.Release.dxilasm", "CalcLight+DiffuseSpecular.Release.dxilasm"};
#else
        const std::string expectedNames[] = {"CalcLight+Diffuse.Debug.dxilasm", "CalcLight+DiffuseSpecular.Debug.dxilasm"};
#endif

        for (size_t i = 0; i < 2; ++i)
        {
            const auto linkedResult =
                Compiler::Link({"main", ShaderStage::PixelShader, testModules[i], sizeof(testModules[i]) / sizeof(testModules[i][0])}, {},
                               {ShadingLanguage::Dxil, ""});

            EXPECT_FALSE(linkedResult.hasError);
            EXPECT_FALSE(linkedResult.isText);

            Compiler::DisassembleDesc disasmDesc;
            disasmDesc.binary = reinterpret_cast<const uint8_t*>(linkedResult.target->Data());
            disasmDesc.binarySize = linkedResult.target->Size();
            disasmDesc.language = ShadingLanguage::Dxil;
            const auto disasmResult = Compiler::Disassemble(disasmDesc);

            const uint8_t* target_ptr = reinterpret_cast<const uint8_t*>(disasmResult.target->Data());
            CompareWithExpected(std::vector<uint8_t>(target_ptr, target_ptr + disasmResult.target->Size()), disasmResult.isText,
                                expectedNames[i]);

            DestroyBlob(linkedResult.errorWarningMsg);
            DestroyBlob(linkedResult.target);
            Compiler::DestroyResultDesc(linkedResult);

            DestroyBlob(disasmResult.errorWarningMsg);
            DestroyBlob(disasmResult.target);
            Compiler::DestroyResultDesc(disasmResult);
        }

        for (auto& mod : dxilModules)
        {
            DestroyBlob(mod.target);
        }
    }
} // namespace

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);

    int retVal = RUN_ALL_TESTS();
    if (retVal != 0)
    {
        getchar();
    }

    return retVal;
}
