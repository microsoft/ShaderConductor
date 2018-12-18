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

#ifndef SHADER_CONDUCTOR_HPP
#define SHADER_CONDUCTOR_HPP

#pragma once

#include <functional>
#include <string>
#include <vector>

#if defined(__clang__)
#define SC_SYMBOL_EXPORT __attribute__((__visibility__("default")))
#define SC_SYMBOL_IMPORT
#elif defined(__GNUC__)
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#define SC_SYMBOL_EXPORT __attribute__((__dllexport__))
#define SC_SYMBOL_IMPORT __attribute__((__dllimport__))
#else
#define SC_SYMBOL_EXPORT __attribute__((__visibility__("default")))
#define SC_SYMBOL_IMPORT
#endif
#elif defined(_MSC_VER)
#define SC_SYMBOL_EXPORT __declspec(dllexport)
#define SC_SYMBOL_IMPORT __declspec(dllimport)
#endif

#ifdef SHADER_CONDUCTOR_SOURCE
#define SC_API SC_SYMBOL_EXPORT
#else
#define SC_API SC_SYMBOL_IMPORT
#endif

namespace ShaderConductor
{
    enum class ShaderStage : uint32_t
    {
        VertexShader,
        PixelShader,
        GeometryShader,
        HullShader,
        DomainShader,
        ComputeShader,

        NumShaderStages,
    };

    enum class ShadingLanguage : uint32_t
    {
        Dxil = 0,
        SpirV,

        Hlsl,
        Glsl,
        Essl,
        Msl,

        NumShadingLanguages,
    };

    struct MacroDefine
    {
        std::string name;
        std::string value;
    };

    class SC_API Compiler
    {
    public:
        struct SourceDesc
        {
            std::string source;
            std::string fileName;
            std::string entryPoint;
            ShaderStage stage;
            std::vector<MacroDefine> defines;
            std::function<std::string(const std::string& includeName)> loadIncludeCallback;
        };


        struct TargetDesc
        {
            ShadingLanguage language;
            std::string version;
        };

        struct ResultDesc
        {
            std::vector<uint8_t> target;
            bool isText;

            std::string errorWarningMsg;
            bool hasError;
        };

        struct DisassembleDesc
        {
            ShadingLanguage language;
            std::vector<uint8_t> binary;
		};

    public:
        static ResultDesc Compile(SourceDesc source, TargetDesc target);
        static ResultDesc Disassemble(DisassembleDesc source);
    };
} // namespace ShaderConductor

#endif // SHADER_CONDUCTOR_HPP
