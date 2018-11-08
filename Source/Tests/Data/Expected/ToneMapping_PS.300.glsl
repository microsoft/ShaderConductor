#version 300
#extension GL_ARB_separate_shader_objects : require

layout(std430) readonly buffer type_StructuredBuffer_float
{
    float _m0[];
} lumBuff;

layout(std140) uniform type_cbPS
{
    float lumStrength;
} cbPS;

uniform sampler2D SPIRV_Cross_CombinedcolorTexpointSampler;
uniform sampler2D SPIRV_Cross_CombinedbloomTexlinearSampler;

layout(location = 0) in vec2 in_var_TEXCOORD0;
out vec4 out_var_SV_Target;

void main()
{
    vec4 _48 = texture(SPIRV_Cross_CombinedcolorTexpointSampler, in_var_TEXCOORD0);
    vec3 _63 = (_48.xyz * (0.7200000286102294921875 / ((lumBuff._m0[0u] * cbPS.lumStrength) + 0.001000000047497451305389404296875))).xyz;
    vec3 _67 = (_63 * (vec3(1.0) + (_63 * vec3(0.666666686534881591796875)))).xyz;
    vec3 _72 = (_67 / (vec3(1.0) + _67)).xyz + (texture(SPIRV_Cross_CombinedbloomTexlinearSampler, in_var_TEXCOORD0).xyz * 0.60000002384185791015625);
    vec4 _74 = vec4(_72.x, _72.y, _72.z, _48.w);
    _74.w = 1.0;
    out_var_SV_Target = _74;
}

