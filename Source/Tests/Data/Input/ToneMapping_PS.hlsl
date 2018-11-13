// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "Common.hlsli"

struct PSInput
{
    float4 pos : SV_Position;
    float2 tex : TEXCOORD0;
};

Texture2D colorTex : register(t0);
Texture2D lumTex : register(t1);
Texture2D bloomTex : register(t2);

static const float MIDDLE_GRAY = 0.72f;
static const float LUM_WHITE = 1.5f;

cbuffer cbPS : register(b0)
{
    float lumStrength;
};

float4 main(PSInput input) : SV_Target
{
    float4 color = colorTex.Sample(pointSampler, input.tex);
    float3 bloom = bloomTex.Sample(linearSampler, input.tex).xyz;
    float lum = lumTex.Sample(pointSampler, 0.5f).x * lumStrength;

    color.rgb *= MIDDLE_GRAY / (lum + 0.001f);
    color.rgb *= (1.0f + color.rgb / LUM_WHITE);
    color.rgb /= (1.0f + color.rgb);
    
    color.rgb += 0.6f * bloom;
    color.a = 1.0f;

    return color;
}
