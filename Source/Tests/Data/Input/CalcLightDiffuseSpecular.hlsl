// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

float SpecularNormalizeFactor(float shininess)
{
    return (shininess + 2) / 8;
}

float3 DistributionTerm(float3 halfway, float3 normal, float shininess)
{
    return pow(max(dot(halfway, normal), 0.0f), shininess);
}

float3 FresnelTerm(float3 lightVec, float3 halfway, float3 specColor)
{
    float eN = saturate(dot(lightVec, halfway));
    return specColor > 0 ? specColor + (1 - specColor) * pow(1 - eN, 5) : 0;
}

float3 SpecularTerm(float3 specColor, float3 lightVec, float3 halfway, float3 normal, float shininess)
{
    return SpecularNormalizeFactor(shininess) * DistributionTerm(halfway, normal, shininess) * FresnelTerm(lightVec, halfway, specColor);
}

// Diffuse and specular
float3 CalcBrdf(float3 diffColor, float3 specColor, float shininess, float3 lightVec, float3 halfway, float3 normal)
{
    return max((diffColor + SpecularTerm(specColor, lightVec, halfway, normal, shininess)) * dot(normal, lightVec), 0);
}
