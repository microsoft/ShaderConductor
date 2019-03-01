// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

float4 DotHalfPS() : SV_Target0
{
	half3 v0 = half3(0, 0, 1);
	float ret = dot(v0, v0);
	return ret;
}
