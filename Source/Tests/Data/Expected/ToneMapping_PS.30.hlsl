cbuffer type_cbPS : register(b0)
{
    float cbPS_lumStrength : packoffset(c0);
};

uniform sampler2D SPIRV_Cross_CombinedcolorTexpointSampler;
uniform sampler2D SPIRV_Cross_CombinedbloomTexlinearSampler;
uniform sampler2D SPIRV_Cross_CombinedlumTexpointSampler;

static float2 in_var_TEXCOORD0;
static float4 out_var_SV_Target;

struct SPIRV_Cross_Input
{
    float2 in_var_TEXCOORD0 : TEXCOORD0;
};

struct SPIRV_Cross_Output
{
    float4 out_var_SV_Target : COLOR0;
};

void frag_main()
{
    float4 _45 = tex2D(SPIRV_Cross_CombinedcolorTexpointSampler, in_var_TEXCOORD0);
    float3 _62 = (_45.xyz * (0.7200000286102294921875f / ((tex2D(SPIRV_Cross_CombinedlumTexpointSampler, 0.5f.xx).x * cbPS_lumStrength) + 0.001000000047497451305389404296875f))).xyz;
    float3 _66 = (_62 * (1.0f.xxx + (_62 * 0.666666686534881591796875f.xxx))).xyz;
    float3 _71 = (_66 / (1.0f.xxx + _66)).xyz + (tex2D(SPIRV_Cross_CombinedbloomTexlinearSampler, in_var_TEXCOORD0).xyz * 0.60000002384185791015625f);
    float4 _73 = float4(_71.x, _71.y, _71.z, _45.w);
    _73.w = 1.0f;
    out_var_SV_Target = _73;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    in_var_TEXCOORD0 = stage_input.in_var_TEXCOORD0;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.out_var_SV_Target = float4(out_var_SV_Target);
    return stage_output;
}
