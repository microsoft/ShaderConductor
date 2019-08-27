// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "Inc/HeaderA.hlsli"

Texture2D colorTex : register(t0);

float4 main(float2 tex : TEXCOORD0) : SV_Target
{
    return colorTex.Sample(pointSampler, tex) + colorTex.Sample(linearSampler, tex);
}
