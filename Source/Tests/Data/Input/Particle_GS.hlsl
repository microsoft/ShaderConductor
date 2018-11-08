// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

cbuffer cbMain : register(b0)
{
    matrix invView;
    matrix viewProj;
};

struct GS_PARTICLE_INPUT
{
    float4 wsPos : POSITION;
};

struct PS_PARTICLE_INPUT
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
};

[maxvertexcount(4)]
void main(point GS_PARTICLE_INPUT input[1], inout TriangleStream<PS_PARTICLE_INPUT> spriteStream)
{
    const float3 quadPositions[4] =
    {
        float3(-1,  1, 0),
        float3( 1,  1, 0),
        float3(-1, -1, 0),
        float3( 1, -1, 0),
    };
    const float2 quadTexcoords[4] = 
    { 
        float2(0, 1), 
        float2(1, 1),
        float2(0, 0),
        float2(1, 0),
    };

    PS_PARTICLE_INPUT output;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float3 position = quadPositions[i] * FIXED_VERTEX_RADIUS;
        position = mul(position, (float3x3)invView) + input[0].wsPos.xyz;
        output.pos = mul(float4(position, 1.0f), viewProj);

        output.tex = quadTexcoords[i];

        spriteStream.Append(output);
    }
    spriteStream.RestartStrip();
}
