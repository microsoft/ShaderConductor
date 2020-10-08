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
        ("I,input", "Input file name", cxxopts::value<std::string>())
        ("O,output", "Output file name", cxxopts::value<std::string>())
        ("S,stage", "Shader stage: vs, ps, gs, hs, ds, cs", cxxopts::value<std::string>())
        ("T,target", "Target shading language: dxil, spirv, hlsl, glsl, essl, msl_macos, msl_ios", cxxopts::value<std::string>()->default_value("dxil"))
        ("V,version", "The version of target shading language", cxxopts::value<std::string>()->default_value(""))
        ("D,define", "Macro define as name=value", cxxopts::value<std::vector<std::string>>())
        ("Mr,rowmajor", "Treat input HLSL matrices as row major, they will be transposed depending on the conventions of the output format", cxxopts::value<bool>()->default_value("false"))
        ("Th,halftypes", "Enable 16bit data types, requires shader model 6.2+", cxxopts::value<bool>()->default_value("false"))
        ("D,debuginfo", "Embed debug info into the binary", cxxopts::value<bool>()->default_value("false"))
        ("Ol,optimization", "Optimization level, 0 to 3, no optimization to most optimization", cxxopts::value<int>()->default_value("3"))
        ("Sma,majorshadermodel", "HLSL shader model major version", cxxopts::value<int>()->default_value("6"))
        ("Smb,minorshadermodel", "HLSL shader model minor version", cxxopts::value<int>()->default_value("0"))
        ("Bst,texturebindshift", "Shift all texture bindings by this value", cxxopts::value<int>()->default_value("0"))
        ("Bss,samplerbindshift", "Shift all sampler bindings by this value", cxxopts::value<int>()->default_value("0"))
        ("Bsc,cbufferbindshift", "Shift all cbuffer bindings by this value", cxxopts::value<int>()->default_value("0"))
        ("Bsu,uabufferbindshift", "Shift all uabuffer bindings by this value", cxxopts::value<int>()->default_value("0"));

    // clang-format on

    auto opts = options.parse(argc, argv);

    if ((opts.count("input") == 0) || (opts.count("stage") == 0))
    {
        std::cerr << "COULDN'T find <input> or <stage> in command line parameters." << std::endl;
        std::cerr << options.help() << std::endl;
        return 1;
    }

    using namespace ShaderConductor;

    Compiler::SourceDesc sourceDesc{};
    Compiler::TargetDesc targetDesc{};

    const auto fileName = opts["input"].as<std::string>();
    const auto targetName = opts["target"].as<std::string>();
    const auto targetVersion = opts["version"].as<std::string>();

    sourceDesc.fileName = fileName.c_str();
    targetDesc.version = targetVersion.empty() ? nullptr : targetVersion.c_str();

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
    else if (targetName == "msl_macos")
    {
        targetDesc.language = ShadingLanguage::Msl_macOS;
    }
    else if (targetName == "msl_ios")
    {
        targetDesc.language = ShadingLanguage::Msl_iOS;
    }
    else
    {
        std::cerr << "Invalid target shading language: " << targetName << std::endl;
        return 1;
    }

    std::string outputName;
    if (opts.count("output") == 0)
    {
        static const std::string extMap[] = {"dxil", "spv", "hlsl", "glsl", "essl", "msl", "msl"};
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
        source.resize(static_cast<size_t>(inputFile.tellg()));
        inputFile.seekg(0, std::ios::beg);
        inputFile.read(&source[0], source.size());
    }
    sourceDesc.source = source.c_str();

    size_t numberOfDefines = opts.count("define");
    std::vector<MacroDefine> macroDefines;
    std::vector<std::string> macroStrings;
    if (numberOfDefines > 0)
    {
        macroDefines.reserve(numberOfDefines);
        macroStrings.reserve(numberOfDefines * 2);
        auto& defines = opts["define"].as<std::vector<std::string>>();
        for (const auto& define : defines)
        {
            MacroDefine macroDefine;
            macroDefine.name = nullptr;
            macroDefine.value = nullptr;

            size_t splitPosition = define.find('=');
            if (splitPosition != std::string::npos)
            {
                std::string macroName = define.substr(0, splitPosition);
                std::string macroValue = define.substr(splitPosition + 1, define.size() - splitPosition - 1);

                macroStrings.push_back(macroName);
                macroDefine.name = macroStrings.back().c_str();
                macroStrings.push_back(macroValue);
                macroDefine.value = macroStrings.back().c_str();
            }
            else
            {
                macroStrings.push_back(define);
                macroDefine.name = macroStrings.back().c_str();
            }

            macroDefines.push_back(macroDefine);
        }

        sourceDesc.defines = macroDefines.data();
        sourceDesc.numDefines = static_cast<uint32_t>(macroDefines.size());
    }

    Compiler::Options compilerOptions{};
    compilerOptions.packMatricesInRowMajor = opts["rowmajor"].as<bool>();
    compilerOptions.enable16bitTypes = opts["halftypes"].as<bool>();
    compilerOptions.enableDebugInfo = opts["debuginfo"].as<bool>();
    compilerOptions.optimizationLevel = opts["optimization"].as<int>();
    compilerOptions.shaderModel.major_ver = opts["majorshadermodel"].as<int>();
    compilerOptions.shaderModel.minor_ver = opts["minorshadermodel"].as<int>();
    compilerOptions.shiftAllTexturesBindings = opts["texturebindshift"].as<int>();
    compilerOptions.shiftAllSamplersBindings = opts["samplerbindshift"].as<int>();
    compilerOptions.shiftAllCBuffersBindings = opts["cbufferbindshift"].as<int>();
    compilerOptions.shiftAllUABuffersBindings = opts["uabufferbindshift"].as<int>();

    try
    {
        const auto result = Compiler::Compile(sourceDesc, compilerOptions, targetDesc);

        if (result.errorWarningMsg.Size() > 0)
        {
            const char* msg = reinterpret_cast<const char*>(result.errorWarningMsg.Data());
            std::cerr << "Error or warning from shader compiler: " << std::endl
                      << std::string(msg, msg + result.errorWarningMsg.Size()) << std::endl;
        }
        if (result.target.Size() > 0)
        {
            std::ofstream outputFile(outputName, std::ios_base::binary);
            if (!outputFile)
            {
                std::cerr << "COULDN'T open the output file: " << outputName << std::endl;
                return 1;
            }

            outputFile.write(reinterpret_cast<const char*>(result.target.Data()), result.target.Size());

            std::cout << "The compiled file is saved to " << outputName << std::endl;
        }
    }
    catch (std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
    }

    return 0;
}
