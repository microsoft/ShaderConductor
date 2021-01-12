#version 300
#extension GL_ARB_compute_shader : require
#extension GL_ARB_separate_shader_objects : require
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

struct Particle
{
    vec2 position;
    vec2 velocity;
};

struct ParticleForces
{
    vec2 acceleration;
};

layout(std140) uniform type_cbSimulationConstants
{
    float timeStep;
    float wallStiffness;
    vec4 gravity;
    vec3 planes[4];
} cbSimulationConstants;

layout(std430) buffer type_RWStructuredBuffer_Particle
{
    Particle _m0[];
} particlesRW;

layout(std430) readonly buffer type_StructuredBuffer_Particle
{
    Particle _m0[];
} particlesRO;

layout(std430) readonly buffer type_StructuredBuffer_ParticleForces
{
    ParticleForces _m0[];
} particlesForcesRO;

void main()
{
    vec2 _52 = particlesRO._m0[gl_GlobalInvocationID.x].position;
    vec2 _54 = particlesRO._m0[gl_GlobalInvocationID.x].velocity;
    vec2 _56 = particlesForcesRO._m0[gl_GlobalInvocationID.x].acceleration;
    vec3 _59 = vec3(_52, 1.0);
    float _67 = -cbSimulationConstants.wallStiffness;
    vec2 _102 = _54 + ((((((_56 + (cbSimulationConstants.planes[0u].xy * (min(dot(_59, cbSimulationConstants.planes[0u]), 0.0) * _67))) + (cbSimulationConstants.planes[1u].xy * (min(dot(_59, cbSimulationConstants.planes[1u]), 0.0) * _67))) + (cbSimulationConstants.planes[2u].xy * (min(dot(_59, cbSimulationConstants.planes[2u]), 0.0) * _67))) + (cbSimulationConstants.planes[3u].xy * (min(dot(_59, cbSimulationConstants.planes[3u]), 0.0) * _67))) + cbSimulationConstants.gravity.xy) * cbSimulationConstants.timeStep);
    particlesRW._m0[gl_GlobalInvocationID.x].position = _52 + (_102 * cbSimulationConstants.timeStep);
    particlesRW._m0[gl_GlobalInvocationID.x].velocity = _102;
}

