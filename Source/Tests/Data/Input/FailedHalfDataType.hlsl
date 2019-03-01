// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

void Func(half3 input, out half3 output)
{
	output = input;
}

float4 HalfOutParamPS() : SV_Target0
{
	float3 output;
	Func(cross(half3(1, 0, 0), half3(0, 1, 0)), output);
	return float4(output, 1.0f);
}
