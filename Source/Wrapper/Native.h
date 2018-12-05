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
    bool isText;
    char* target;
    int targetSize;
    bool hasError;
    char* errorWarningMsg;
};

extern "C" __declspec(dllexport) void Compile(SourceDescription* source, TargetDescription* target, ResultDescription* result);
