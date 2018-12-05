// ShaderConductorWrapper.cpp : Defines the exported functions for the DLL application.
//

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