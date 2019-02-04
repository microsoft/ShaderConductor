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
        const char* name;
        const char* value;
    };

    class SC_API Blob
    {
    public:
        virtual ~Blob();

        virtual const void* Data() const = 0;
        virtual uint32_t Size() const = 0;
    };

    SC_API Blob* CreateBlob(const void* data, uint32_t size);
    SC_API void DestroyBlob(Blob* blob);

    class SC_API Compiler
    {
    public:
        struct SourceDesc
        {
            const char* source;
            const char* fileName;
            const char* entryPoint;
            ShaderStage stage;
            const MacroDefine* defines;
            uint32_t numDefines;
            std::function<Blob*(const char* includeName)> loadIncludeCallback;
        };

        struct TargetDesc
        {
            ShadingLanguage language;
            const char* version;
        };

        struct ResultDesc
        {
            Blob* target;
            bool isText;

            Blob* errorWarningMsg;
            bool hasError;
        };

        struct DisassembleDesc
        {
            ShadingLanguage language;
            uint8_t* binary;
            uint32_t binarySize;
        };

    public:
        static ResultDesc Compile(const SourceDesc& source, const TargetDesc& target);
        static ResultDesc Disassemble(const DisassembleDesc& source);
    };
} // namespace ShaderConductor

#endif // SHADER_CONDUCTOR_HPP
