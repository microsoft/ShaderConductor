#pragma once
#include <ShaderConductor/ShaderConductor.hpp>

using namespace ShaderConductor;

 struct SourceDescription
{
    const char* source;    
    const char* entryPoint;
    ShaderStage stage;    
};

struct TargetDescription
{
    ShadingLanguage shadingLanguage;
    const char* version;
};

 struct ResultDescription
{   
    bool hasError;
    char* target;
    char* errorWarningMsg;
};

extern "C" __declspec(dllexport) void Compile(SourceDescription* source, TargetDescription* target, ResultDescription* result);
