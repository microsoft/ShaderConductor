// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Diffuse only
float3 CalcBrdf(float3 diffColor, float3 specColor, float shininess, float3 lightVec, float3 halfway, float3 normal)
{
    return max(diffColor * dot(normal, lightVec), 0);
}
