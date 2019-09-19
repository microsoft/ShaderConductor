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

#pragma once

#include <ShaderConductor/ShaderConductor.hpp>

using namespace ShaderConductor;

struct ShaderConductorBlob;

struct SourceDescription
{
    const char* source;
    const char* entryPoint;
    ShaderStage stage;
};

struct ShaderModel
{
    int major;
    int minor;
};

struct OptionsDescription
{
    bool packMatricesInRowMajor = true; // Experimental: Decide how a matrix get packed
    bool enable16bitTypes = false;      // Enable 16-bit types, such as half, uint16_t. Requires shader model 6.2+
    bool enableDebugInfo = false;       // Embed debug info into the binary
    bool disableOptimizations = false;  // Force to turn off optimizations. Ignore optimizationLevel below.
    int optimizationLevel = 3;          // 0 to 3, no optimization to most optimization

    ShaderModel shaderModel = { 6, 0 };

    int shiftAllTexturesBindings;
    int shiftAllSamplersBindings;
    int shiftAllCBuffersBindings;
    int shiftAllUABuffersBindings;
};

struct TargetDescription
{
    ShadingLanguage shadingLanguage;
    const char* version;
};

struct ResultDescription
{
    ShaderConductorBlob* target;
    bool isText;

    ShaderConductorBlob* errorWarningMsg;
    bool hasError;
};

struct DisassembleDescription
{
    ShadingLanguage language;
    char* binary;
    int binarySize;
};

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

SC_SYMBOL_EXPORT void Compile(SourceDescription* source, OptionsDescription* optionsDesc, TargetDescription* target, ResultDescription* result);
SC_SYMBOL_EXPORT void Disassemble(DisassembleDescription* source, ResultDescription* result);

SC_SYMBOL_EXPORT ShaderConductorBlob* CreateShaderConductorBlob(const void* data, int size);
SC_SYMBOL_EXPORT void DestroyShaderConductorBlob(ShaderConductorBlob* blob);
SC_SYMBOL_EXPORT const void* GetShaderConductorBlobData(ShaderConductorBlob* blob);
SC_SYMBOL_EXPORT int GetShaderConductorBlobSize(ShaderConductorBlob* blob);
