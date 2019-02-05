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

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4819)
#endif
#include <cxxopts.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

int main(int argc, char** argv)
{
    cxxopts::Options options("ShaderConductorCmd", "A tool for compiling HLSL to many shader languages.");
    // clang-format off
    options.add_options()
        ("E,entry", "Entry point of the shader", cxxopts::value<std::string>()->default_value("main"))
        ("I,input", "Input file name", cxxopts::value<std::string>())("O,output", "Output file name", cxxopts::value<std::string>())
        ("S,stage", "Shader stage: vs, ps, gs, hs, ds, cs", cxxopts::value<std::string>())
        ("T,target", "Target shading language: dxil, spirv, hlsl, glsl, essl, msl", cxxopts::value<std::string>()->default_value("dxil"))
        ("V,version", "The version of target shading language", cxxopts::value<std::string>()->default_value(""));
    // clang-format on

    auto opts = options.parse(argc, argv);

    if ((opts.count("input") == 0) || (opts.count("stage") == 0))
    {
        std::cerr << "COULDN'T find <input> or <stage> in command line parameters." << std::endl;
        std::cerr << options.help() << std::endl;
        return 1;
    }

    using namespace ShaderConductor;

    Compiler::SourceDesc sourceDesc;
    Compiler::TargetDesc targetDesc;

    const auto fileName = opts["input"].as<std::string>();
    const auto targetName = opts["target"].as<std::string>();
    const auto targetVersion = opts["version"].as<std::string>();

    sourceDesc.fileName = fileName.c_str();
    targetDesc.version = targetVersion.c_str();

    const auto stageName = opts["stage"].as<std::string>();
    if (stageName == "vs")
    {
        sourceDesc.stage = ShaderStage::VertexShader;
    }
    else if (stageName == "ps")
    {
        sourceDesc.stage = ShaderStage::PixelShader;
    }
    else if (stageName == "gs")
    {
        sourceDesc.stage = ShaderStage::GeometryShader;
    }
    else if (stageName == "hs")
    {
        sourceDesc.stage = ShaderStage::HullShader;
    }
    else if (stageName == "ds")
    {
        sourceDesc.stage = ShaderStage::DomainShader;
    }
    else if (stageName == "cs")
    {
        sourceDesc.stage = ShaderStage::ComputeShader;
    }
    else
    {
        std::cerr << "Invalid shader stage: " << stageName << std::endl;
        return 1;
    }

    const auto entryPoint = opts["entry"].as<std::string>();
    sourceDesc.entryPoint = entryPoint.c_str();

    if (targetName == "dxil")
    {
        targetDesc.language = ShadingLanguage::Dxil;
    }
    else if (targetName == "spirv")
    {
        targetDesc.language = ShadingLanguage::SpirV;
    }
    else if (targetName == "hlsl")
    {
        targetDesc.language = ShadingLanguage::Hlsl;
    }
    else if (targetName == "glsl")
    {
        targetDesc.language = ShadingLanguage::Glsl;
    }
    else if (targetName == "essl")
    {
        targetDesc.language = ShadingLanguage::Essl;
    }
    else if (targetName == "msl")
    {
        targetDesc.language = ShadingLanguage::Msl;
    }
    else
    {
        std::cerr << "Invalid target shading language: " << targetName << std::endl;
        return 1;
    }

    std::string outputName;
    if (opts.count("output") == 0)
    {
        static const std::string extMap[] = { "dxil", "spv", "hlsl", "glsl", "essl", "msl" };
        static_assert(sizeof(extMap) / sizeof(extMap[0]) == static_cast<uint32_t>(ShadingLanguage::NumShadingLanguages),
                      "extMap doesn't match with the number of shading languages.");
        outputName = fileName + "." + extMap[static_cast<uint32_t>(targetDesc.language)];
    }
    else
    {
        outputName = opts["output"].as<std::string>();
    }

    std::string source;
    {
        std::ifstream inputFile(sourceDesc.fileName, std::ios_base::binary);
        if (!inputFile)
        {
            std::cerr << "COULDN'T load the input file: " << sourceDesc.fileName << std::endl;
            return 1;
        }

        inputFile.seekg(0, std::ios::end);
        source.resize(inputFile.tellg());
        inputFile.seekg(0, std::ios::beg);
        inputFile.read(&source[0], source.size());
    }
    sourceDesc.source = source.c_str();

    try
    {
        const auto result = Compiler::Compile(sourceDesc, {}, targetDesc);

        if (result.errorWarningMsg != nullptr)
        {
            const char* msg = reinterpret_cast<const char*>(result.errorWarningMsg->Data());
            std::cerr << "Error or warning form shader compiler: " << std::endl
                      << std::string(msg, msg + result.errorWarningMsg->Size()) << std::endl;
        }
        if (result.target != nullptr)
        {
            std::ofstream outputFile(outputName, std::ios_base::binary);
            if (!outputFile)
            {
                std::cerr << "COULDN'T open the output file: " << outputName << std::endl;
                return 1;
            }

            outputFile.write(reinterpret_cast<const char*>(result.target->Data()), result.target->Size());

            std::cout << "The compiled file is saved to " << outputName << std::endl;
        }

        DestroyBlob(result.errorWarningMsg);
        DestroyBlob(result.target);
    }
    catch (std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
    }

    return 0;
}
