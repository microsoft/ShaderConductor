// ShaderConductorWrapper.cpp : Defines the exported functions for the DLL application.
//

#include "Native.h"
#include <ShaderConductor/ShaderConductor.hpp>
#include <iostream>

using namespace ShaderConductor;

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

            const size_t errorDataLen = strlen(errorData);
            result->errorWarningMsg = new char[errorDataLen + 1]();
            std::strncpy(result->errorWarningMsg, errorData, errorDataLen);
        }
        if (!translation.target.empty())
        {
            const char* targetData = reinterpret_cast<const char*>(translation.target.data());					
			
            const size_t targetDataLength = translation.target.size();
            result->targetSize = targetDataLength;
            result->target = new char[targetDataLength + 1]();
            std::strncpy(result->target, targetData, targetDataLength);
        }
		
        result->hasError = translation.hasError;
        result->isText = translation.isText;
    }
    catch (std::exception& ex)
    {
        const char* exception = ex.what();
				
		const size_t exceptionLength = strlen(exception);
        result->errorWarningMsg = new char[exceptionLength + 1]();
        std::strncpy(result->errorWarningMsg, exception, exceptionLength);

		result->hasError = true;
    }
}