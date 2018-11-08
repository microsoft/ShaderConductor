// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

struct Particle
{
    float2 position;
    float2 velocity;
};

struct ParticleForces
{
    float2 acceleration;
};

cbuffer cbSimulationConstants : register(b0)
{
    float timeStep;
    float wallStiffness;

    float4 gravity;
    float3 planes[4];
};

RWStructuredBuffer<Particle> particlesRW : register(u0);
StructuredBuffer<Particle> particlesRO : register(t0);

StructuredBuffer<ParticleForces> particlesForcesRO : register(t2);

[numthreads(256, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID, uint gi : SV_GroupIndex)
{
    const uint p_id = dtid.x;

    float2 position = particlesRO[p_id].position;
    float2 velocity = particlesRO[p_id].velocity;
    float2 acceleration = particlesForcesRO[p_id].acceleration;

    [unroll]
    for (uint i = 0 ; i < 4 ; ++i)
    {
        float dist = dot(float3(position, 1), planes[i]);
        acceleration += min(dist, 0) * -wallStiffness * planes[i].xy;
    }

    acceleration += gravity.xy;

    velocity += timeStep * acceleration;
    position += timeStep * velocity;

    particlesRW[p_id].position = position;
    particlesRW[p_id].velocity = velocity;
}
