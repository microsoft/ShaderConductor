// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

struct PSInput
{
    float4 pos : SV_Position;
    float3 normal : NORMAL;
    float3 lightVec : TEXCOORD0;
    float3 halfway : TEXCOORD1;
};

cbuffer cbPS : register(b0)
{
    float3 diffColor;
    float3 specColor;
    float shininess;
};

float3 CalcBrdf(float3 diffColor, float3 specColor, float shininess, float3 lightVec, float3 halfway, float3 normal);

[shader("pixel")]
float4 main(PSInput input) : SV_Target
{
    float4 color;
    color.rgb = CalcBrdf(diffColor, specColor, shininess, input.lightVec, input.halfway, input.normal);
    color.a = 1.0f;

    return color;
}
