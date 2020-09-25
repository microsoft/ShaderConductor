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

#include "Native.h"
#include <ShaderConductor/ShaderConductor.hpp>
#include <iostream>

using namespace ShaderConductor;

void Compile(SourceDescription* source, OptionsDescription* optionsDesc, TargetDescription* target, ResultDescription* result)
{
    Compiler::SourceDesc sourceDesc;
    sourceDesc.entryPoint = source->entryPoint;
    sourceDesc.source = source->source;
    sourceDesc.stage = source->stage;
    sourceDesc.fileName = nullptr;
    sourceDesc.defines = nullptr;
    sourceDesc.numDefines = 0;

    Compiler::Options options;
    options.packMatricesInRowMajor = optionsDesc->packMatricesInRowMajor;
    options.enable16bitTypes = optionsDesc->enable16bitTypes;
    options.enableDebugInfo = optionsDesc->enableDebugInfo;
    options.disableOptimizations = optionsDesc->disableOptimizations;
    options.optimizationLevel = optionsDesc->optimizationLevel;
    options.shaderModel = {static_cast<uint8_t>(optionsDesc->shaderModel.major), static_cast<uint8_t>(optionsDesc->shaderModel.minor)};
    options.shiftAllTexturesBindings = optionsDesc->shiftAllTexturesBindings;
    options.shiftAllSamplersBindings = optionsDesc->shiftAllSamplersBindings;
    options.shiftAllCBuffersBindings = optionsDesc->shiftAllCBuffersBindings;
    options.shiftAllUABuffersBindings = optionsDesc->shiftAllUABuffersBindings;

    Compiler::TargetDesc targetDesc{};
    targetDesc.language = target->shadingLanguage;
    targetDesc.version = target->version;

    try
    {
        const auto translation = Compiler::Compile(sourceDesc, options, targetDesc);

        if (translation.errorWarningMsg.Size() > 0)
        {
            result->errorWarningMsg = CreateShaderConductorBlob(translation.errorWarningMsg.Data(), translation.errorWarningMsg.Size());
        }
        if (translation.target.Size() > 0)
        {
            result->target = CreateShaderConductorBlob(translation.target.Data(), translation.target.Size());
        }

        result->hasError = translation.hasError;
        result->isText = translation.isText;
    }
    catch (std::exception& ex)
    {
        const char* exception = ex.what();
        result->errorWarningMsg = CreateShaderConductorBlob(exception, static_cast<uint32_t>(strlen(exception)));
        result->hasError = true;
    }
}

void Disassemble(DisassembleDescription* source, ResultDescription* result)
{
    Compiler::DisassembleDesc disassembleSource;
    disassembleSource.language = source->language;
    disassembleSource.binary = reinterpret_cast<uint8_t*>(source->binary);
    disassembleSource.binarySize = source->binarySize;

    const auto disassembleResult = Compiler::Disassemble(disassembleSource);

    if (disassembleResult.errorWarningMsg.Size() > 0)
    {
        result->errorWarningMsg =
            CreateShaderConductorBlob(disassembleResult.errorWarningMsg.Data(), disassembleResult.errorWarningMsg.Size());
    }
    if (disassembleResult.target.Size() > 0)
    {
        result->target = CreateShaderConductorBlob(disassembleResult.target.Data(), disassembleResult.target.Size());
    }

    result->hasError = disassembleResult.hasError;
    result->isText = disassembleResult.isText;
}

ShaderConductorBlob* CreateShaderConductorBlob(const void* data, int size)
{
    return reinterpret_cast<ShaderConductorBlob*>(new ShaderConductor::Blob(data, size));
}

void DestroyShaderConductorBlob(ShaderConductorBlob* blob)
{
    delete reinterpret_cast<Blob*>(blob);
}

const void* GetShaderConductorBlobData(ShaderConductorBlob* blob)
{
    return reinterpret_cast<Blob*>(blob)->Data();
}

int GetShaderConductorBlobSize(ShaderConductorBlob* blob)
{
    return reinterpret_cast<Blob*>(blob)->Size();
}
