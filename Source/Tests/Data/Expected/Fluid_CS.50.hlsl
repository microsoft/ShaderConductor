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
    float cbSimulationConstants_timeStep : packoffset(c0);
    float cbSimulationConstants_wallStiffness : packoffset(c0.y);
    float4 cbSimulationConstants_gravity : packoffset(c1);
    float3 cbSimulationConstants_planes[4] : packoffset(c2);
};
RWByteAddressBuffer particlesRW : register(u0);
ByteAddressBuffer particlesRO : register(t0);
ByteAddressBuffer particlesForcesRO : register(t2);

static uint3 gl_GlobalInvocationID;
struct SPIRV_Cross_Input
{
    uint3 gl_GlobalInvocationID : SV_DispatchThreadID;
};

void comp_main()
{
    float2 _51 = asfloat(particlesRO.Load2(gl_GlobalInvocationID.x * 16 + 0));
    float2 _53 = asfloat(particlesRO.Load2(gl_GlobalInvocationID.x * 16 + 8));
    float2 _57;
    _57 = asfloat(particlesForcesRO.Load2(gl_GlobalInvocationID.x * 8 + 0));
    [unroll]
    for (uint _60 = 0u; _60 < 4u; )
    {
        _57 += (cbSimulationConstants_planes[_60].xy * (min(dot(float3(_51, 1.0f), cbSimulationConstants_planes[_60]), 0.0f) * (-cbSimulationConstants_wallStiffness)));
        _60++;
        continue;
    }
    float2 _84 = _53 + ((_57 + cbSimulationConstants_gravity.xy) * cbSimulationConstants_timeStep);
    particlesRW.Store2(gl_GlobalInvocationID.x * 16 + 0, asuint(_51 + (_84 * cbSimulationConstants_timeStep)));
    particlesRW.Store2(gl_GlobalInvocationID.x * 16 + 8, asuint(_84));
}

[numthreads(256, 1, 1)]
void main(SPIRV_Cross_Input stage_input)
{
    gl_GlobalInvocationID = stage_input.gl_GlobalInvocationID;
    comp_main();
}
