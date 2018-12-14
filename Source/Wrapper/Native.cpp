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

char* CopyString(const char* source, int length = -1)
{
    size_t sourceLength;
    if (length < 0)
    {
        sourceLength = strlen(source);
    }
	else
	{
        sourceLength = length;
	}

    char* result = new char[sourceLength + 1];
    strncpy_s(result, sourceLength + 1, source, sourceLength);

    return result;
}

void Compile(SourceDescription* source, TargetDescription* target, ResultDescription* result)
{
    Compiler::SourceDesc sourceDesc;
    sourceDesc.entryPoint = source->entryPoint;
    sourceDesc.source = source->source;
    sourceDesc.stage = source->stage;

    Compiler::TargetDesc targetDesc;
    targetDesc.language = target->shadingLanguage;
    targetDesc.version = target->version;
    targetDesc.disassemble = target->disassemble;

    try
    {
        const auto translation = Compiler::Compile(std::move(sourceDesc), std::move(targetDesc));

        if (!translation.errorWarningMsg.empty())
        {
            const char* errorData = translation.errorWarningMsg.c_str();
            result->errorWarningMsg = CopyString(errorData);
        }
        if (!translation.target.empty())
        {
            const char* targetData = reinterpret_cast<const char*>(translation.target.data());
            result->targetSize = static_cast<int>(translation.target.size());
            result->target = CopyString(targetData, result->targetSize);
        }

        result->hasError = translation.hasError;
        result->isText = translation.isText;
    }
    catch (std::exception& ex)
    {
        const char* exception = ex.what();      
        result->errorWarningMsg = CopyString(exception);
        result->hasError = true;
    }
}
