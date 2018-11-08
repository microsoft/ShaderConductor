// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

void VSMain(float4 pos : POSITION,
            float2 tex : TEXCOORD0,
            out float2 oTex : TEXCOORD0,
            out float4 oPos : SV_Position)
{
    oTex = tex;
    oPos = pos;
}
