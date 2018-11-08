// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

cbuffer cbPNTriangles : register(b0)
{
    float4x4 viewProj;
    float4 lightDir;
}

struct HS_ConstantOutput
{
    float tessFactor[3] : SV_TessFactor;
    float insideTessFactor : SV_InsideTessFactor;

    float3 b210 : POSITION3;
    float3 b120 : POSITION4;
    float3 b021 : POSITION5;
    float3 b012 : POSITION6;
    float3 b102 : POSITION7;
    float3 b201 : POSITION8;
    float3 b111 : CENTER;
};

struct HS_ControlPointOutput
{
    float3 position : POSITION;
    float2 texCoord : TEXCOORD;
};

struct DS_Output
{
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
};

[domain("tri")]
DS_Output main(HS_ConstantOutput hsConstantData, const OutputPatch<HS_ControlPointOutput, 3> input,
               float3 barycentricCoords : SV_DomainLocation)
{
    DS_Output output;

    float u = barycentricCoords.x;
    float v = barycentricCoords.y;
    float w = barycentricCoords.z;

    float uu = u * u;
    float vv = v * v;
    float ww = w * w;
    float uu3 = uu * 3;
    float vv3 = vv * 3;
    float ww3 = ww * 3;

    float3 position = input[0].position * ww * w +
                      input[1].position * uu * u +
                      input[2].position * vv * v +
                      hsConstantData.b210 * ww3 * u +
                      hsConstantData.b120 * w * uu3 +
                      hsConstantData.b201 * ww3 * v +
                      hsConstantData.b021 * uu3 * v +
                      hsConstantData.b102 * w * vv3 +
                      hsConstantData.b012 * u * vv3 +
                      hsConstantData.b111 * 6 * w * u * v;

    output.position = mul(float4(position, 1), viewProj);

    output.texCoord = input[0].texCoord * w + input[1].texCoord * u + input[2].texCoord * v;

    return output;
}
