#version 300
#extension GL_ARB_separate_shader_objects : require

layout(std140) uniform type_cbPS
{
    float lumStrength;
} cbPS;

uniform sampler2D SPIRV_Cross_CombinedcolorTexpointSampler;
uniform sampler2D SPIRV_Cross_CombinedbloomTexlinearSampler;
uniform sampler2D SPIRV_Cross_CombinedlumTexpointSampler;

layout(location = 0) in vec2 varying_TEXCOORD0;
out vec4 out_var_SV_Target;

void main()
{
    vec4 _45 = texture(SPIRV_Cross_CombinedcolorTexpointSampler, varying_TEXCOORD0);
    vec3 _62 = (_45.xyz * (0.7200000286102294921875 / ((texture(SPIRV_Cross_CombinedlumTexpointSampler, vec2(0.5)).x * cbPS.lumStrength) + 0.001000000047497451305389404296875))).xyz;
    vec3 _66 = (_62 * (vec3(1.0) + (_62 * vec3(0.666666686534881591796875)))).xyz;
    vec3 _71 = (_66 / (vec3(1.0) + _66)).xyz + (texture(SPIRV_Cross_CombinedbloomTexlinearSampler, varying_TEXCOORD0).xyz * 0.60000002384185791015625);
    vec4 _73 = vec4(_71.x, _71.y, _71.z, _45.w);
    _73.w = 1.0;
    out_var_SV_Target = _73;
}

