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

#include "Common.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace ShaderConductor;

namespace
{
    TEST(ReflectionTest, VertexShader)
    {
        const std::string fileName = TEST_DATA_DIR "Input/Transform_VS.hlsl";

        std::vector<uint8_t> input = LoadFile(fileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        Compiler::Options options{};
        options.needReflection = true;

        const auto result =
            Compiler::Compile({source.c_str(), fileName.c_str(), "main", ShaderStage::VertexShader}, options, {ShadingLanguage::Dxil, ""});

        EXPECT_FALSE(result.hasError);
        EXPECT_FALSE(result.isText);

        if (!result.reflection.Valid())
        {
            GTEST_SKIP_("Dxil Reflection is not supported on this platform");
        }

        EXPECT_EQ(result.reflection.NumInputParameters(), 1U);
        {
            const Reflection::SignatureParameterDesc* inputParam = result.reflection.InputParameter(0);
            EXPECT_NE(inputParam, nullptr);
            EXPECT_STRCASEEQ(inputParam->semantic, "POSITION");
            EXPECT_EQ(inputParam->semanticIndex, 0U);
            EXPECT_EQ(inputParam->location, 0U);
            EXPECT_EQ(inputParam->componentType, Reflection::VariableType::DataType::Float);
            EXPECT_EQ(inputParam->mask, Reflection::ComponentMask::X | Reflection::ComponentMask::Y | Reflection::ComponentMask::Z |
                                            Reflection::ComponentMask::W);
        }
        EXPECT_EQ(result.reflection.InputParameter(1), nullptr);

        EXPECT_EQ(result.reflection.NumOutputParameters(), 1U);
        {
            const Reflection::SignatureParameterDesc* outputParam = result.reflection.OutputParameter(0);
            EXPECT_NE(outputParam, nullptr);
            EXPECT_STRCASEEQ(outputParam->semantic, "SV_Position");
            EXPECT_EQ(outputParam->semanticIndex, 0U);
            EXPECT_EQ(outputParam->location, 0U);
            EXPECT_EQ(outputParam->componentType, Reflection::VariableType::DataType::Float);
            EXPECT_EQ(outputParam->mask, Reflection::ComponentMask::X | Reflection::ComponentMask::Y | Reflection::ComponentMask::Z |
                                             Reflection::ComponentMask::W);
        }
        EXPECT_EQ(result.reflection.OutputParameter(1), nullptr);

        EXPECT_EQ(result.reflection.GSHSInputPrimitive(), Reflection::PrimitiveTopology::Undefined);
        EXPECT_EQ(result.reflection.GSOutputTopology(), Reflection::PrimitiveTopology::Undefined);
        EXPECT_EQ(result.reflection.GSMaxNumOutputVertices(), 0U);
        EXPECT_EQ(result.reflection.GSNumInstances(), 0U);

        EXPECT_EQ(result.reflection.HSOutputPrimitive(), Reflection::TessellatorOutputPrimitive::Undefined);
        EXPECT_EQ(result.reflection.HSPartitioning(), Reflection::TessellatorPartitioning::Undefined);

        EXPECT_EQ(result.reflection.HSDSTessellatorDomain(), Reflection::TessellatorDomain::Undefined);
        EXPECT_EQ(result.reflection.HSDSNumPatchConstantParameters(), 0U);
        EXPECT_EQ(result.reflection.HSDSNumConrolPoints(), 0U);

        EXPECT_EQ(result.reflection.CSBlockSizeX(), 0U);
        EXPECT_EQ(result.reflection.CSBlockSizeY(), 0U);
        EXPECT_EQ(result.reflection.CSBlockSizeZ(), 0U);

        EXPECT_EQ(result.reflection.NumResources(), 1U);
        {
            const Reflection::ResourceDesc* resource = result.reflection.ResourceByIndex(0);
            EXPECT_NE(resource, nullptr);
            EXPECT_STREQ(resource->name, "cbVS");
            EXPECT_EQ(resource->type, ShaderResourceType::ConstantBuffer);
            EXPECT_EQ(resource->space, 0U);
            EXPECT_EQ(resource->bindPoint, 0U);
            EXPECT_EQ(resource->bindCount, 1U);
            EXPECT_EQ(result.reflection.ResourceByIndex(1), nullptr);

            const Reflection::ConstantBuffer* cbuffer = result.reflection.ConstantBufferByIndex(0);
            EXPECT_NE(cbuffer, nullptr);
            EXPECT_STREQ(cbuffer->Name(), "cbVS");
            EXPECT_EQ(cbuffer->Size(), 64U);

            EXPECT_EQ(cbuffer->NumVariables(), 1U);
            {
                const Reflection::VariableDesc* variable = cbuffer->VariableByIndex(0);
                EXPECT_NE(variable, nullptr);
                EXPECT_STREQ(variable->name, "wvp");
                EXPECT_STREQ(variable->type.Name(), "float4x4");
                EXPECT_EQ(variable->type.Type(), Reflection::VariableType::DataType::Float);
                EXPECT_EQ(variable->type.Rows(), 4U);
                EXPECT_EQ(variable->type.Columns(), 4U);
                EXPECT_EQ(variable->type.Elements(), 0U);
                EXPECT_EQ(variable->type.ElementStride(), 0U);
                EXPECT_EQ(variable->offset, 0U);
                EXPECT_EQ(variable->size, 64U);
            }
            EXPECT_EQ(result.reflection.ConstantBufferByIndex(1), nullptr);
        }
    }

    TEST(ReflectionTest, HullShader)
    {
        const std::string fileName = TEST_DATA_DIR "Input/DetailTessellation_HS.hlsl";

        std::vector<uint8_t> input = LoadFile(fileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        Compiler::Options options{};
        options.needReflection = true;

        const auto result =
            Compiler::Compile({source.c_str(), fileName.c_str(), "main", ShaderStage::HullShader}, options, {ShadingLanguage::Dxil, ""});

        EXPECT_FALSE(result.hasError);
        EXPECT_FALSE(result.isText);

        if (!result.reflection.Valid())
        {
            GTEST_SKIP_("Dxil Reflection is not supported on this platform");
        }

        EXPECT_EQ(result.reflection.NumInputParameters(), 4U);
        {
            {
                const Reflection::SignatureParameterDesc* inputParam = result.reflection.InputParameter(0);
                EXPECT_NE(inputParam, nullptr);
                EXPECT_STRCASEEQ(inputParam->semantic, "WORLDPOS");
                EXPECT_EQ(inputParam->semanticIndex, 0U);
                EXPECT_EQ(inputParam->location, 0U);
                EXPECT_EQ(inputParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(inputParam->mask, Reflection::ComponentMask::X | Reflection::ComponentMask::Y | Reflection::ComponentMask::Z);
            }
            {
                const Reflection::SignatureParameterDesc* inputParam = result.reflection.InputParameter(1);
                EXPECT_NE(inputParam, nullptr);
                EXPECT_STRCASEEQ(inputParam->semantic, "NORMAL");
                EXPECT_EQ(inputParam->semanticIndex, 0U);
                EXPECT_EQ(inputParam->location, 1U);
                EXPECT_EQ(inputParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(inputParam->mask, Reflection::ComponentMask::X | Reflection::ComponentMask::Y | Reflection::ComponentMask::Z);
            }
            {
                const Reflection::SignatureParameterDesc* inputParam = result.reflection.InputParameter(2);
                EXPECT_NE(inputParam, nullptr);
                EXPECT_STRCASEEQ(inputParam->semantic, "TEXCOORD");
                EXPECT_EQ(inputParam->semanticIndex, 0U);
                EXPECT_EQ(inputParam->location, 2U);
                EXPECT_EQ(inputParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(inputParam->mask, Reflection::ComponentMask::X | Reflection::ComponentMask::Y);
            }
            {
                const Reflection::SignatureParameterDesc* inputParam = result.reflection.InputParameter(3);
                EXPECT_NE(inputParam, nullptr);
                EXPECT_STRCASEEQ(inputParam->semantic, "LIGHTVECTORTS");
                EXPECT_EQ(inputParam->semanticIndex, 0U);
                EXPECT_EQ(inputParam->location, 3U);
                EXPECT_EQ(inputParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(inputParam->mask, Reflection::ComponentMask::X | Reflection::ComponentMask::Y | Reflection::ComponentMask::Z);
            }
        }

        EXPECT_EQ(result.reflection.NumOutputParameters(), 4U);
        {
            {
                const Reflection::SignatureParameterDesc* outputParam = result.reflection.OutputParameter(0);
                EXPECT_NE(outputParam, nullptr);
                EXPECT_STRCASEEQ(outputParam->semantic, "WORLDPOS");
                EXPECT_EQ(outputParam->semanticIndex, 0U);
                EXPECT_EQ(outputParam->location, 0U);
                EXPECT_EQ(outputParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(outputParam->mask, Reflection::ComponentMask::X | Reflection::ComponentMask::Y | Reflection::ComponentMask::Z);
            }
            {
                const Reflection::SignatureParameterDesc* outputParam = result.reflection.OutputParameter(1);
                EXPECT_NE(outputParam, nullptr);
                EXPECT_STRCASEEQ(outputParam->semantic, "NORMAL");
                EXPECT_EQ(outputParam->semanticIndex, 0U);
                EXPECT_EQ(outputParam->location, 1U);
                EXPECT_EQ(outputParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(outputParam->mask, Reflection::ComponentMask::X | Reflection::ComponentMask::Y | Reflection::ComponentMask::Z);
            }
            {
                const Reflection::SignatureParameterDesc* outputParam = result.reflection.OutputParameter(2);
                EXPECT_NE(outputParam, nullptr);
                EXPECT_STRCASEEQ(outputParam->semantic, "TEXCOORD");
                EXPECT_EQ(outputParam->semanticIndex, 0U);
                EXPECT_EQ(outputParam->location, 2U);
                EXPECT_EQ(outputParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(outputParam->mask, Reflection::ComponentMask::X | Reflection::ComponentMask::Y);
            }
            {
                const Reflection::SignatureParameterDesc* outputParam = result.reflection.OutputParameter(3);
                EXPECT_NE(outputParam, nullptr);
                EXPECT_STRCASEEQ(outputParam->semantic, "LIGHTVECTORTS");
                EXPECT_EQ(outputParam->semanticIndex, 0U);
                EXPECT_EQ(outputParam->location, 3U);
                EXPECT_EQ(outputParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(outputParam->mask, Reflection::ComponentMask::X | Reflection::ComponentMask::Y | Reflection::ComponentMask::Z);
            }
        }

        EXPECT_EQ(result.reflection.GSHSInputPrimitive(), Reflection::PrimitiveTopology::Patches_3_CtrlPoint);
        EXPECT_EQ(result.reflection.GSOutputTopology(), Reflection::PrimitiveTopology::Undefined);
        EXPECT_EQ(result.reflection.GSMaxNumOutputVertices(), 0U);
        EXPECT_EQ(result.reflection.GSNumInstances(), 0U);

        EXPECT_EQ(result.reflection.HSOutputPrimitive(), Reflection::TessellatorOutputPrimitive::TriangleCW);
        EXPECT_EQ(result.reflection.HSPartitioning(), Reflection::TessellatorPartitioning::FractionalOdd);

        EXPECT_EQ(result.reflection.HSDSTessellatorDomain(), Reflection::TessellatorDomain::Triangle);
        EXPECT_EQ(result.reflection.HSDSNumPatchConstantParameters(), 4U);
        {
            for (uint32_t i = 0; i < 3; ++i)
            {
                const Reflection::SignatureParameterDesc* patchConstantParam = result.reflection.HSDSPatchConstantParameter(i);
                EXPECT_NE(patchConstantParam, nullptr);
                EXPECT_STRCASEEQ(patchConstantParam->semantic, "SV_TessFactor");
                EXPECT_EQ(patchConstantParam->semanticIndex, i);
                EXPECT_EQ(patchConstantParam->location, i);
                EXPECT_EQ(patchConstantParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(patchConstantParam->mask, Reflection::ComponentMask::W);
            }
            {
                const Reflection::SignatureParameterDesc* patchConstantParam = result.reflection.HSDSPatchConstantParameter(3);
                EXPECT_NE(patchConstantParam, nullptr);
                EXPECT_STRCASEEQ(patchConstantParam->semantic, "SV_InsideTessFactor");
                EXPECT_EQ(patchConstantParam->semanticIndex, 0U);
                EXPECT_EQ(patchConstantParam->location, 3U);
                EXPECT_EQ(patchConstantParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(patchConstantParam->mask, Reflection::ComponentMask::X);
            }
        }
        EXPECT_EQ(result.reflection.HSDSNumConrolPoints(), 3U);

        EXPECT_EQ(result.reflection.CSBlockSizeX(), 0U);
        EXPECT_EQ(result.reflection.CSBlockSizeY(), 0U);
        EXPECT_EQ(result.reflection.CSBlockSizeZ(), 0U);

        EXPECT_EQ(result.reflection.NumResources(), 1U);
        {
            const Reflection::ResourceDesc* resource = result.reflection.ResourceByIndex(0);
            EXPECT_NE(resource, nullptr);
            EXPECT_STREQ(resource->name, "cbMain");
            EXPECT_EQ(resource->type, ShaderResourceType::ConstantBuffer);
            EXPECT_EQ(resource->space, 0U);
            EXPECT_EQ(resource->bindPoint, 0U);
            EXPECT_EQ(resource->bindCount, 1U);

            const Reflection::ConstantBuffer* cbuffer = result.reflection.ConstantBufferByIndex(0);
            EXPECT_NE(cbuffer, nullptr);
            EXPECT_STRCASEEQ(cbuffer->Name(), "cbMain");
            EXPECT_EQ(cbuffer->Size(), 16U);

            EXPECT_EQ(cbuffer->NumVariables(), 1U);
            {
                const Reflection::VariableDesc* variable = cbuffer->VariableByIndex(0);
                EXPECT_NE(variable, nullptr);
                EXPECT_STREQ(variable->name, "tessellationFactor");
                EXPECT_STREQ(variable->type.Name(), "float4");
                EXPECT_EQ(variable->type.Type(), Reflection::VariableType::DataType::Float);
                EXPECT_EQ(variable->type.Rows(), 1U);
                EXPECT_EQ(variable->type.Columns(), 4U);
                EXPECT_EQ(variable->type.Elements(), 0U);
                EXPECT_EQ(variable->type.ElementStride(), 0U);
                EXPECT_EQ(variable->offset, 0U);
                EXPECT_EQ(variable->size, 16U);
            }
        }
    }

    TEST(ReflectionTest, DomainShader)
    {
        const std::string fileName = TEST_DATA_DIR "Input/PNTriangles_DS.hlsl";

        std::vector<uint8_t> input = LoadFile(fileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        Compiler::Options options{};
        options.needReflection = true;

        const auto result =
            Compiler::Compile({source.c_str(), fileName.c_str(), "main", ShaderStage::DomainShader}, options, {ShadingLanguage::Dxil, ""});

        EXPECT_FALSE(result.hasError);
        EXPECT_FALSE(result.isText);

        if (!result.reflection.Valid())
        {
            GTEST_SKIP_("Dxil Reflection is not supported on this platform");
        }

        EXPECT_EQ(result.reflection.NumInputParameters(), 2U);
        {
            {
                const Reflection::SignatureParameterDesc* inputParam = result.reflection.InputParameter(0);
                EXPECT_NE(inputParam, nullptr);
                EXPECT_STRCASEEQ(inputParam->semantic, "POSITION");
                EXPECT_EQ(inputParam->semanticIndex, 0U);
                EXPECT_EQ(inputParam->location, 0U);
                EXPECT_EQ(inputParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(inputParam->mask, Reflection::ComponentMask::X | Reflection::ComponentMask::Y | Reflection::ComponentMask::Z);
            }
            {
                const Reflection::SignatureParameterDesc* inputParam = result.reflection.InputParameter(1);
                EXPECT_NE(inputParam, nullptr);
                EXPECT_STRCASEEQ(inputParam->semantic, "TEXCOORD");
                EXPECT_EQ(inputParam->semanticIndex, 0U);
                EXPECT_EQ(inputParam->location, 1U);
                EXPECT_EQ(inputParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(inputParam->mask, Reflection::ComponentMask::X | Reflection::ComponentMask::Y);
            }
        }

        EXPECT_EQ(result.reflection.NumOutputParameters(), 2U);
        {
            {
                const Reflection::SignatureParameterDesc* outputParam = result.reflection.OutputParameter(0);
                EXPECT_NE(outputParam, nullptr);
                EXPECT_STRCASEEQ(outputParam->semantic, "SV_Position");
                EXPECT_EQ(outputParam->semanticIndex, 0U);
                EXPECT_EQ(outputParam->location, 0U);
                EXPECT_EQ(outputParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(outputParam->mask, Reflection::ComponentMask::X | Reflection::ComponentMask::Y | Reflection::ComponentMask::Z |
                                                 Reflection::ComponentMask::W);
            }
            {
                const Reflection::SignatureParameterDesc* outputParam = result.reflection.OutputParameter(1);
                EXPECT_NE(outputParam, nullptr);
                EXPECT_STRCASEEQ(outputParam->semantic, "TEXCOORD");
                EXPECT_EQ(outputParam->semanticIndex, 0U);
                EXPECT_EQ(outputParam->location, 1U);
                EXPECT_EQ(outputParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(outputParam->mask, Reflection::ComponentMask::X | Reflection::ComponentMask::Y);
            }
        }

        EXPECT_EQ(result.reflection.GSHSInputPrimitive(), Reflection::PrimitiveTopology::Undefined);
        EXPECT_EQ(result.reflection.GSOutputTopology(), Reflection::PrimitiveTopology::Undefined);
        EXPECT_EQ(result.reflection.GSMaxNumOutputVertices(), 0U);
        EXPECT_EQ(result.reflection.GSNumInstances(), 0U);

        EXPECT_EQ(result.reflection.HSOutputPrimitive(), Reflection::TessellatorOutputPrimitive::Undefined);
        EXPECT_EQ(result.reflection.HSPartitioning(), Reflection::TessellatorPartitioning::Undefined);

        EXPECT_EQ(result.reflection.HSDSTessellatorDomain(), Reflection::TessellatorDomain::Triangle);
        EXPECT_EQ(result.reflection.HSDSNumPatchConstantParameters(), 11U);
        {
            for (uint32_t i = 0; i < 3; ++i)
            {
                const Reflection::SignatureParameterDesc* patchConstantParam = result.reflection.HSDSPatchConstantParameter(i);
                EXPECT_NE(patchConstantParam, nullptr);
                EXPECT_STRCASEEQ(patchConstantParam->semantic, "SV_TessFactor");
                EXPECT_EQ(patchConstantParam->semanticIndex, i);
                EXPECT_EQ(patchConstantParam->location, i);
                EXPECT_EQ(patchConstantParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(patchConstantParam->mask, Reflection::ComponentMask::W);
            }
            {
                const Reflection::SignatureParameterDesc* patchConstantParam = result.reflection.HSDSPatchConstantParameter(3);
                EXPECT_NE(patchConstantParam, nullptr);
                EXPECT_STRCASEEQ(patchConstantParam->semantic, "SV_InsideTessFactor");
                EXPECT_EQ(patchConstantParam->semanticIndex, 0U);
                EXPECT_EQ(patchConstantParam->location, 3U);
                EXPECT_EQ(patchConstantParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(patchConstantParam->mask, Reflection::ComponentMask::X);
            }

            const uint32_t locations[] = {0, 1, 2, 4, 5, 6};
            for (uint32_t i = 0; i < 6; ++i)
            {
                const Reflection::SignatureParameterDesc* patchConstantParam = result.reflection.HSDSPatchConstantParameter(i + 4);
                EXPECT_NE(patchConstantParam, nullptr);
                EXPECT_STRCASEEQ(patchConstantParam->semantic, "POSITION");
                EXPECT_EQ(patchConstantParam->semanticIndex, i + 3);
                EXPECT_EQ(patchConstantParam->location, locations[i]);
                EXPECT_EQ(patchConstantParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(patchConstantParam->mask,
                          Reflection::ComponentMask::X | Reflection::ComponentMask::Y | Reflection::ComponentMask::Z);
            }
            {
                const Reflection::SignatureParameterDesc* patchConstantParam = result.reflection.HSDSPatchConstantParameter(10);
                EXPECT_NE(patchConstantParam, nullptr);
                EXPECT_STRCASEEQ(patchConstantParam->semantic, "CENTER");
                EXPECT_EQ(patchConstantParam->semanticIndex, 0U);
                EXPECT_EQ(patchConstantParam->location, 7U);
                EXPECT_EQ(patchConstantParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(patchConstantParam->mask,
                          Reflection::ComponentMask::X | Reflection::ComponentMask::Y | Reflection::ComponentMask::Z);
            }
        }
        EXPECT_EQ(result.reflection.HSDSNumConrolPoints(), 3U);

        EXPECT_EQ(result.reflection.CSBlockSizeX(), 0U);
        EXPECT_EQ(result.reflection.CSBlockSizeY(), 0U);
        EXPECT_EQ(result.reflection.CSBlockSizeZ(), 0U);

        EXPECT_EQ(result.reflection.NumResources(), 1U);
        {
            const Reflection::ResourceDesc* resource = result.reflection.ResourceByIndex(0);
            EXPECT_NE(resource, nullptr);
            EXPECT_STREQ(resource->name, "cbPNTriangles");
            EXPECT_EQ(resource->type, ShaderResourceType::ConstantBuffer);
            EXPECT_EQ(resource->space, 0U);
            EXPECT_EQ(resource->bindPoint, 0U);
            EXPECT_EQ(resource->bindCount, 1U);

            const Reflection::ConstantBuffer* cbuffer = result.reflection.ConstantBufferByIndex(0);
            EXPECT_NE(cbuffer, nullptr);
            EXPECT_STREQ(cbuffer->Name(), "cbPNTriangles");
            EXPECT_EQ(cbuffer->Size(), 80U);

            EXPECT_EQ(cbuffer->NumVariables(), 2U);
            {
                const Reflection::VariableDesc* variable = cbuffer->VariableByIndex(0);
                EXPECT_NE(variable, nullptr);
                EXPECT_STREQ(variable->name, "viewProj");
                EXPECT_STREQ(variable->type.Name(), "float4x4");
                EXPECT_EQ(variable->type.Type(), Reflection::VariableType::DataType::Float);
                EXPECT_EQ(variable->type.Rows(), 4U);
                EXPECT_EQ(variable->type.Columns(), 4U);
                EXPECT_EQ(variable->type.Elements(), 0U);
                EXPECT_EQ(variable->type.ElementStride(), 0U);
                EXPECT_EQ(variable->offset, 0U);
                EXPECT_EQ(variable->size, 64U);
            }
            {
                const Reflection::VariableDesc* variable = cbuffer->VariableByIndex(1);
                EXPECT_NE(variable, nullptr);
                EXPECT_STREQ(variable->name, "lightDir");
                EXPECT_STREQ(variable->type.Name(), "float4");
                EXPECT_EQ(variable->type.Type(), Reflection::VariableType::DataType::Float);
                EXPECT_EQ(variable->type.Rows(), 1U);
                EXPECT_EQ(variable->type.Columns(), 4U);
                EXPECT_EQ(variable->type.Elements(), 0U);
                EXPECT_EQ(variable->type.ElementStride(), 0U);
                EXPECT_EQ(variable->offset, 64U);
                EXPECT_EQ(variable->size, 16U);
            }
        }
    }

    TEST(ReflectionTest, GeometryShader)
    {
        const std::string fileName = TEST_DATA_DIR "Input/Particle_GS.hlsl";

        std::vector<uint8_t> input = LoadFile(fileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        Compiler::Options options{};
        options.needReflection = true;

        std::vector<MacroDefine> defines = {{"FIXED_VERTEX_RADIUS", "5.0"}};
        const auto result = Compiler::Compile(
            {source.c_str(), fileName.c_str(), "main", ShaderStage::GeometryShader, defines.data(), static_cast<uint32_t>(defines.size())},
            options, {ShadingLanguage::Dxil, ""});

        EXPECT_FALSE(result.hasError);
        EXPECT_FALSE(result.isText);

        if (!result.reflection.Valid())
        {
            GTEST_SKIP_("Dxil Reflection is not supported on this platform");
        }

        EXPECT_EQ(result.reflection.NumInputParameters(), 1U);
        {
            const Reflection::SignatureParameterDesc* inputParam = result.reflection.InputParameter(0);
            EXPECT_NE(inputParam, nullptr);
            EXPECT_STRCASEEQ(inputParam->semantic, "POSITION");
            EXPECT_EQ(inputParam->semanticIndex, 0U);
            EXPECT_EQ(inputParam->location, 0U);
            EXPECT_EQ(inputParam->componentType, Reflection::VariableType::DataType::Float);
            EXPECT_EQ(inputParam->mask, Reflection::ComponentMask::X | Reflection::ComponentMask::Y | Reflection::ComponentMask::Z |
                                            Reflection::ComponentMask::W);
        }

        EXPECT_EQ(result.reflection.NumOutputParameters(), 2U);
        {
            {
                const Reflection::SignatureParameterDesc* outputParam = result.reflection.OutputParameter(0);
                EXPECT_NE(outputParam, nullptr);
                EXPECT_STRCASEEQ(outputParam->semantic, "SV_Position");
                EXPECT_EQ(outputParam->semanticIndex, 0U);
                EXPECT_EQ(outputParam->location, 0U);
                EXPECT_EQ(outputParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(outputParam->mask, Reflection::ComponentMask::X | Reflection::ComponentMask::Y | Reflection::ComponentMask::Z |
                                                 Reflection::ComponentMask::W);
            }
            {
                const Reflection::SignatureParameterDesc* outputParam = result.reflection.OutputParameter(1);
                EXPECT_NE(outputParam, nullptr);
                EXPECT_STRCASEEQ(outputParam->semantic, "TEXCOORD");
                EXPECT_EQ(outputParam->semanticIndex, 0U);
                EXPECT_EQ(outputParam->location, 1U);
                EXPECT_EQ(outputParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(outputParam->mask, Reflection::ComponentMask::X | Reflection::ComponentMask::Y);
            }
        }

        EXPECT_EQ(result.reflection.GSHSInputPrimitive(), Reflection::PrimitiveTopology::Points);
        EXPECT_EQ(result.reflection.GSOutputTopology(), Reflection::PrimitiveTopology::TriangleStrip);
        EXPECT_EQ(result.reflection.GSMaxNumOutputVertices(), 4U);
        EXPECT_EQ(result.reflection.GSNumInstances(), 1U);

        EXPECT_EQ(result.reflection.HSOutputPrimitive(), Reflection::TessellatorOutputPrimitive::Undefined);
        EXPECT_EQ(result.reflection.HSPartitioning(), Reflection::TessellatorPartitioning::Undefined);

        EXPECT_EQ(result.reflection.HSDSTessellatorDomain(), Reflection::TessellatorDomain::Undefined);
        EXPECT_EQ(result.reflection.HSDSNumPatchConstantParameters(), 0U);
        EXPECT_EQ(result.reflection.HSDSNumConrolPoints(), 0U);

        EXPECT_EQ(result.reflection.CSBlockSizeX(), 0U);
        EXPECT_EQ(result.reflection.CSBlockSizeY(), 0U);
        EXPECT_EQ(result.reflection.CSBlockSizeZ(), 0U);

        EXPECT_EQ(result.reflection.NumResources(), 1U);
        {
            const Reflection::ResourceDesc* resource = result.reflection.ResourceByIndex(0);
            EXPECT_NE(resource, nullptr);
            EXPECT_STREQ(resource->name, "cbMain");
            EXPECT_EQ(resource->type, ShaderResourceType::ConstantBuffer);
            EXPECT_EQ(resource->space, 0U);
            EXPECT_EQ(resource->bindPoint, 0U);
            EXPECT_EQ(resource->bindCount, 1U);

            const Reflection::ConstantBuffer* cbuffer = result.reflection.ConstantBufferByIndex(0);
            EXPECT_NE(cbuffer, nullptr);
            EXPECT_STREQ(cbuffer->Name(), "cbMain");
            EXPECT_EQ(cbuffer->Size(), 128U);

            EXPECT_EQ(cbuffer->NumVariables(), 2U);
            {
                const Reflection::VariableDesc* variable = cbuffer->VariableByIndex(0);
                EXPECT_NE(variable, nullptr);
                EXPECT_STREQ(variable->name, "invView");
                EXPECT_STREQ(variable->type.Name(), "float4x4");
                EXPECT_EQ(variable->type.Type(), Reflection::VariableType::DataType::Float);
                EXPECT_EQ(variable->type.Rows(), 4U);
                EXPECT_EQ(variable->type.Columns(), 4U);
                EXPECT_EQ(variable->type.Elements(), 0U);
                EXPECT_EQ(variable->type.ElementStride(), 0U);
                EXPECT_EQ(variable->offset, 0U);
                EXPECT_EQ(variable->size, 64U);
            }
            {
                const Reflection::VariableDesc* variable = cbuffer->VariableByIndex(1);
                EXPECT_NE(variable, nullptr);
                EXPECT_STREQ(variable->name, "viewProj");
                EXPECT_STREQ(variable->type.Name(), "float4x4");
                EXPECT_EQ(variable->type.Type(), Reflection::VariableType::DataType::Float);
                EXPECT_EQ(variable->type.Rows(), 4U);
                EXPECT_EQ(variable->type.Columns(), 4U);
                EXPECT_EQ(variable->type.Elements(), 0U);
                EXPECT_EQ(variable->type.ElementStride(), 0U);
                EXPECT_EQ(variable->offset, 64U);
                EXPECT_EQ(variable->size, 64U);
            }
        }
    }

    TEST(ReflectionTest, PixelShader)
    {
        const std::string fileName = TEST_DATA_DIR "Input/ToneMapping_PS.hlsl";

        std::vector<uint8_t> input = LoadFile(fileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        Compiler::Options options{};
        options.needReflection = true;

        const auto result =
            Compiler::Compile({source.c_str(), fileName.c_str(), "main", ShaderStage::PixelShader}, options, {ShadingLanguage::Dxil, ""});

        EXPECT_FALSE(result.hasError);
        EXPECT_FALSE(result.isText);

        if (!result.reflection.Valid())
        {
            GTEST_SKIP_("Dxil Reflection is not supported on this platform");
        }

        EXPECT_EQ(result.reflection.NumInputParameters(), 2U);
        {
            {
                const Reflection::SignatureParameterDesc* inputParam = result.reflection.InputParameter(0);
                EXPECT_NE(inputParam, nullptr);
                EXPECT_STRCASEEQ(inputParam->semantic, "SV_Position");
                EXPECT_EQ(inputParam->semanticIndex, 0U);
                EXPECT_EQ(inputParam->location, 0U);
                EXPECT_EQ(inputParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(inputParam->mask, Reflection::ComponentMask::X | Reflection::ComponentMask::Y | Reflection::ComponentMask::Z |
                                                Reflection::ComponentMask::W);
            }
            {
                const Reflection::SignatureParameterDesc* inputParam = result.reflection.InputParameter(1);
                EXPECT_NE(inputParam, nullptr);
                EXPECT_STRCASEEQ(inputParam->semantic, "TEXCOORD");
                EXPECT_EQ(inputParam->semanticIndex, 0U);
                EXPECT_EQ(inputParam->location, 1U);
                EXPECT_EQ(inputParam->componentType, Reflection::VariableType::DataType::Float);
                EXPECT_EQ(inputParam->mask, Reflection::ComponentMask::X | Reflection::ComponentMask::Y);
            }
        }

        EXPECT_EQ(result.reflection.NumOutputParameters(), 1U);
        {
            const Reflection::SignatureParameterDesc* outputParam = result.reflection.OutputParameter(0);
            EXPECT_NE(outputParam, nullptr);
            EXPECT_STRCASEEQ(outputParam->semantic, "SV_Target");
            EXPECT_EQ(outputParam->semanticIndex, 0U);
            EXPECT_EQ(outputParam->location, 0U);
            EXPECT_EQ(outputParam->componentType, Reflection::VariableType::DataType::Float);
            EXPECT_EQ(outputParam->mask, Reflection::ComponentMask::X | Reflection::ComponentMask::Y | Reflection::ComponentMask::Z |
                                             Reflection::ComponentMask::W);
        }

        EXPECT_EQ(result.reflection.GSHSInputPrimitive(), Reflection::PrimitiveTopology::Undefined);
        EXPECT_EQ(result.reflection.GSOutputTopology(), Reflection::PrimitiveTopology::Undefined);
        EXPECT_EQ(result.reflection.GSMaxNumOutputVertices(), 0U);
        EXPECT_EQ(result.reflection.GSNumInstances(), 0U);

        EXPECT_EQ(result.reflection.HSOutputPrimitive(), Reflection::TessellatorOutputPrimitive::Undefined);
        EXPECT_EQ(result.reflection.HSPartitioning(), Reflection::TessellatorPartitioning::Undefined);

        EXPECT_EQ(result.reflection.HSDSTessellatorDomain(), Reflection::TessellatorDomain::Undefined);
        EXPECT_EQ(result.reflection.HSDSNumPatchConstantParameters(), 0U);
        EXPECT_EQ(result.reflection.HSDSNumConrolPoints(), 0U);

        EXPECT_EQ(result.reflection.CSBlockSizeX(), 0U);
        EXPECT_EQ(result.reflection.CSBlockSizeY(), 0U);
        EXPECT_EQ(result.reflection.CSBlockSizeZ(), 0U);

        EXPECT_EQ(result.reflection.NumResources(), 6U);
        {
            {
                const Reflection::ResourceDesc* resource = result.reflection.ResourceByName("cbPS");
                EXPECT_NE(resource, nullptr);
                EXPECT_STREQ(resource->name, "cbPS");
                EXPECT_EQ(resource->type, ShaderResourceType::ConstantBuffer);
                EXPECT_EQ(resource->space, 0U);
                EXPECT_EQ(resource->bindPoint, 0U);
                EXPECT_EQ(resource->bindCount, 1U);

                const Reflection::ConstantBuffer* cbuffer = result.reflection.ConstantBufferByIndex(0);
                EXPECT_NE(cbuffer, nullptr);
                EXPECT_STREQ(cbuffer->Name(), "cbPS");
                EXPECT_EQ(cbuffer->Size(), 16U);

                EXPECT_EQ(cbuffer->NumVariables(), 1U);
                {
                    const Reflection::VariableDesc* variable = cbuffer->VariableByIndex(0);
                    EXPECT_NE(variable, nullptr);
                    EXPECT_STREQ(variable->name, "lumStrength");
                    EXPECT_STREQ(variable->type.Name(), "float");
                    EXPECT_EQ(variable->type.Type(), Reflection::VariableType::DataType::Float);
                    EXPECT_EQ(variable->type.Rows(), 1U);
                    EXPECT_EQ(variable->type.Columns(), 1U);
                    EXPECT_EQ(variable->type.Elements(), 0U);
                    EXPECT_EQ(variable->type.ElementStride(), 0U);
                    EXPECT_EQ(variable->offset, 0U);
                    EXPECT_EQ(variable->size, 4U);
                }
            }
            {
                const Reflection::ResourceDesc* resource = result.reflection.ResourceByName("pointSampler");
                EXPECT_NE(resource, nullptr);
                EXPECT_STREQ(resource->name, "pointSampler");
                EXPECT_EQ(resource->type, ShaderResourceType::Sampler);
                EXPECT_EQ(resource->space, 0U);
                EXPECT_EQ(resource->bindPoint, 0U);
                EXPECT_EQ(resource->bindCount, 1U);
            }
            {
                const Reflection::ResourceDesc* resource = result.reflection.ResourceByName("linearSampler");
                EXPECT_NE(resource, nullptr);
                EXPECT_STREQ(resource->name, "linearSampler");
                EXPECT_EQ(resource->type, ShaderResourceType::Sampler);
                EXPECT_EQ(resource->space, 0U);
                EXPECT_EQ(resource->bindPoint, 1U);
                EXPECT_EQ(resource->bindCount, 1U);
            }
            {
                const Reflection::ResourceDesc* resource = result.reflection.ResourceByName("colorTex");
                EXPECT_NE(resource, nullptr);
                EXPECT_STREQ(resource->name, "colorTex");
                EXPECT_EQ(resource->type, ShaderResourceType::Texture);
                EXPECT_EQ(resource->space, 0U);
                EXPECT_EQ(resource->bindPoint, 0U);
                EXPECT_EQ(resource->bindCount, 1U);
            }
            {
                const Reflection::ResourceDesc* resource = result.reflection.ResourceByName("lumTex");
                EXPECT_NE(resource, nullptr);
                EXPECT_STREQ(resource->name, "lumTex");
                EXPECT_EQ(resource->type, ShaderResourceType::Texture);
                EXPECT_EQ(resource->space, 0U);
                EXPECT_EQ(resource->bindPoint, 1U);
                EXPECT_EQ(resource->bindCount, 1U);
            }
            {
                const Reflection::ResourceDesc* resource = result.reflection.ResourceByName("bloomTex");
                EXPECT_NE(resource, nullptr);
                EXPECT_STREQ(resource->name, "bloomTex");
                EXPECT_EQ(resource->type, ShaderResourceType::Texture);
                EXPECT_EQ(resource->space, 0U);
                EXPECT_EQ(resource->bindPoint, 2U);
                EXPECT_EQ(resource->bindCount, 1U);
            }
            EXPECT_EQ(result.reflection.ResourceByName("NotExists"), nullptr);
        }
    }

    TEST(ReflectionTest, ComputeShader)
    {
        const std::string fileName = TEST_DATA_DIR "Input/Fluid_CS.hlsl";

        std::vector<uint8_t> input = LoadFile(fileName, true);
        const std::string source = std::string(reinterpret_cast<char*>(input.data()), input.size());

        Compiler::Options options{};
        options.needReflection = true;

        const auto result =
            Compiler::Compile({source.c_str(), fileName.c_str(), "main", ShaderStage::ComputeShader}, options, {ShadingLanguage::Dxil, ""});

        EXPECT_FALSE(result.hasError);
        EXPECT_FALSE(result.isText);

        if (!result.reflection.Valid())
        {
            GTEST_SKIP_("Dxil Reflection is not supported on this platform");
        }

        EXPECT_EQ(result.reflection.NumInputParameters(), 0U);
        EXPECT_EQ(result.reflection.NumOutputParameters(), 0U);

        EXPECT_EQ(result.reflection.GSHSInputPrimitive(), Reflection::PrimitiveTopology::Undefined);
        EXPECT_EQ(result.reflection.GSOutputTopology(), Reflection::PrimitiveTopology::Undefined);
        EXPECT_EQ(result.reflection.GSMaxNumOutputVertices(), 0U);
        EXPECT_EQ(result.reflection.GSNumInstances(), 0U);

        EXPECT_EQ(result.reflection.HSOutputPrimitive(), Reflection::TessellatorOutputPrimitive::Undefined);
        EXPECT_EQ(result.reflection.HSPartitioning(), Reflection::TessellatorPartitioning::Undefined);

        EXPECT_EQ(result.reflection.HSDSTessellatorDomain(), Reflection::TessellatorDomain::Undefined);
        EXPECT_EQ(result.reflection.HSDSNumPatchConstantParameters(), 0U);
        EXPECT_EQ(result.reflection.HSDSNumConrolPoints(), 0U);

        EXPECT_EQ(result.reflection.CSBlockSizeX(), 256U);
        EXPECT_EQ(result.reflection.CSBlockSizeY(), 1U);
        EXPECT_EQ(result.reflection.CSBlockSizeZ(), 1U);

        EXPECT_EQ(result.reflection.NumResources(), 4U);
        {
            {
                const Reflection::ResourceDesc* resource = result.reflection.ResourceByName("cbSimulationConstants");
                EXPECT_NE(resource, nullptr);
                EXPECT_STREQ(resource->name, "cbSimulationConstants");
                EXPECT_EQ(resource->type, ShaderResourceType::ConstantBuffer);
                EXPECT_EQ(resource->space, 0U);
                EXPECT_EQ(resource->bindPoint, 0U);
                EXPECT_EQ(resource->bindCount, 1U);

                const Reflection::ConstantBuffer* cbuffer = result.reflection.ConstantBufferByIndex(0);
                EXPECT_NE(cbuffer, nullptr);
                EXPECT_STREQ(cbuffer->Name(), "cbSimulationConstants");
                EXPECT_EQ(cbuffer->Size(), 112U);

                EXPECT_EQ(cbuffer->NumVariables(), 2U);
                {
                    const Reflection::VariableDesc* variable = cbuffer->VariableByIndex(0);
                    EXPECT_NE(variable, nullptr);
                    EXPECT_STREQ(variable->name, "timeStep");
                    EXPECT_STREQ(variable->type.Name(), "float");
                    EXPECT_EQ(variable->type.Type(), Reflection::VariableType::DataType::Float);
                    EXPECT_EQ(variable->type.Rows(), 1U);
                    EXPECT_EQ(variable->type.Columns(), 1U);
                    EXPECT_EQ(variable->type.Elements(), 0U);
                    EXPECT_EQ(variable->type.ElementStride(), 0U);
                    EXPECT_EQ(variable->offset, 0U);
                    EXPECT_EQ(variable->size, 4U);
                }
                {
                    const Reflection::VariableDesc* variable = cbuffer->VariableByIndex(1);
                    EXPECT_NE(variable, nullptr);
                    EXPECT_STREQ(variable->name, "scene");
                    EXPECT_STREQ(variable->type.Name(), "Scene");
                    EXPECT_EQ(variable->type.Type(), Reflection::VariableType::DataType::Struct);
                    EXPECT_EQ(variable->type.Rows(), 1U);
                    EXPECT_EQ(variable->type.Columns(), 17U);
                    EXPECT_EQ(variable->type.Elements(), 0U);
                    EXPECT_EQ(variable->type.ElementStride(), 0U);
                    EXPECT_EQ(variable->type.NumMembers(), 3U);
                    EXPECT_EQ(variable->offset, 16U);
                    EXPECT_EQ(variable->size, 92U);

                    {
                        const Reflection::VariableDesc* member = variable->type.MemberByIndex(0);
                        EXPECT_NE(member, nullptr);
                        EXPECT_STREQ(member->name, "wallStiffness");
                        EXPECT_STREQ(member->type.Name(), "float");
                        EXPECT_EQ(member->type.Type(), Reflection::VariableType::DataType::Float);
                        EXPECT_EQ(member->type.Rows(), 1U);
                        EXPECT_EQ(member->type.Columns(), 1U);
                        EXPECT_EQ(member->type.Elements(), 0U);
                        EXPECT_EQ(member->type.ElementStride(), 0U);
                        EXPECT_EQ(member->offset, 0U);
                        EXPECT_EQ(member->size, 4U);
                    }
                    {
                        const Reflection::VariableDesc* member = variable->type.MemberByIndex(1);
                        EXPECT_NE(member, nullptr);
                        EXPECT_STREQ(member->name, "gravity");
                        EXPECT_STREQ(member->type.Name(), "float4");
                        EXPECT_EQ(member->type.Type(), Reflection::VariableType::DataType::Float);
                        EXPECT_EQ(member->type.Rows(), 1U);
                        EXPECT_EQ(member->type.Columns(), 4U);
                        EXPECT_EQ(member->type.Elements(), 0U);
                        EXPECT_EQ(member->type.ElementStride(), 0U);
                        EXPECT_EQ(member->offset, 16U);
                        EXPECT_EQ(member->size, 16U);
                    }
                    {
                        const Reflection::VariableDesc* member = variable->type.MemberByIndex(2);
                        EXPECT_NE(member, nullptr);
                        EXPECT_STREQ(member->name, "planes");
                        EXPECT_STREQ(member->type.Name(), "float3");
                        EXPECT_EQ(member->type.Type(), Reflection::VariableType::DataType::Float);
                        EXPECT_EQ(member->type.Rows(), 1U);
                        EXPECT_EQ(member->type.Columns(), 3U);
                        EXPECT_EQ(member->type.Elements(), 4U);
                        EXPECT_EQ(member->type.ElementStride(), 16U);
                        EXPECT_EQ(member->offset, 32U);
                        EXPECT_EQ(member->size, 60U);
                    }
                }
            }
            {
                const Reflection::ResourceDesc* resource = result.reflection.ResourceByName("particlesRO");
                EXPECT_NE(resource, nullptr);
                EXPECT_STREQ(resource->name, "particlesRO");
                EXPECT_EQ(resource->type, ShaderResourceType::ShaderResourceView);
                EXPECT_EQ(resource->space, 0U);
                EXPECT_EQ(resource->bindPoint, 0U);
                EXPECT_EQ(resource->bindCount, 1U);
            }
            {
                const Reflection::ResourceDesc* resource = result.reflection.ResourceByName("particlesForcesRO");
                EXPECT_NE(resource, nullptr);
                EXPECT_STREQ(resource->name, "particlesForcesRO");
                EXPECT_EQ(resource->type, ShaderResourceType::ShaderResourceView);
                EXPECT_EQ(resource->space, 0U);
                EXPECT_EQ(resource->bindPoint, 2U);
                EXPECT_EQ(resource->bindCount, 1U);
            }
            {
                const Reflection::ResourceDesc* resource = result.reflection.ResourceByName("particlesRW");
                EXPECT_NE(resource, nullptr);
                EXPECT_STREQ(resource->name, "particlesRW");
                EXPECT_EQ(resource->type, ShaderResourceType::UnorderedAccessView);
                EXPECT_EQ(resource->space, 0U);
                EXPECT_EQ(resource->bindPoint, 0U);
                EXPECT_EQ(resource->bindCount, 1U);
            }
        }
    }
} // namespace
