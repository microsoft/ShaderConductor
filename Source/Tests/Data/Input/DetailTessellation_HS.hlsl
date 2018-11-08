// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

cbuffer cbMain : register(b0)
{
    float4 tessellationFactor;
};

struct VS_OUTPUT_HS_INPUT
{
    float3 worldPos : WORLDPOS;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
    float3 lightTS : LIGHTVECTORTS;
};

struct HS_CONSTANT_DATA_OUTPUT
{
    float edges[3] : SV_TessFactor;
    float inside : SV_InsideTessFactor;
};

struct HS_CONTROL_POINT_OUTPUT
{
    float3 worldPos : WORLDPOS;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
    float3 lightTS : LIGHTVECTORTS;
};

HS_CONSTANT_DATA_OUTPUT ConstantsHS(InputPatch<VS_OUTPUT_HS_INPUT, 3> p, uint patchID : SV_PrimitiveID)
{
    HS_CONSTANT_DATA_OUTPUT output;
    
    output.edges[0] = tessellationFactor.x;
    output.edges[1] = tessellationFactor.y;
    output.edges[2] = tessellationFactor.z;
    output.inside = tessellationFactor.w;
    
    return output;
}

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("ConstantsHS")]
[maxtessfactor(15.0)]
HS_CONTROL_POINT_OUTPUT main(InputPatch<VS_OUTPUT_HS_INPUT, 3> inputPatch, uint uCPID : SV_OutputControlPointID)
{
    HS_CONTROL_POINT_OUTPUT output;

    output.worldPos = inputPatch[uCPID].worldPos.xyz;
    output.normal = inputPatch[uCPID].normal;
    output.texCoord = inputPatch[uCPID].texCoord;
    output.lightTS = inputPatch[uCPID].lightTS;

    return output;
}
