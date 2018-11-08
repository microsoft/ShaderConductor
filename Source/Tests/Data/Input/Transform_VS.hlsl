// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

cbuffer cbVS : register(b0)
{
    float4x4 wvp;
};

void main(float4 pos : POSITION,
          out float4 oPos : SV_Position)
{
    oPos = mul(pos, wvp);
}
