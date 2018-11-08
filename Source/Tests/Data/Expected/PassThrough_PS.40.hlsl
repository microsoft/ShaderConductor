static float4 in_var_COLOR;
static float4 out_var_SV_Target;

struct SPIRV_Cross_Input
{
    float4 in_var_COLOR : TEXCOORD0;
};

struct SPIRV_Cross_Output
{
    float4 out_var_SV_Target : SV_Target0;
};

void frag_main()
{
    out_var_SV_Target = in_var_COLOR;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    in_var_COLOR = stage_input.in_var_COLOR;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.out_var_SV_Target = out_var_SV_Target;
    return stage_output;
}
