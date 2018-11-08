static float4 out_var_SV_Target;

struct SPIRV_Cross_Output
{
    float4 out_var_SV_Target : SV_Target0;
};

void frag_main()
{
    out_var_SV_Target = float4(0.20000000298023223876953125f, 0.4000000059604644775390625f, 0.60000002384185791015625f, 1.0f);
}

SPIRV_Cross_Output main()
{
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.out_var_SV_Target = out_var_SV_Target;
    return stage_output;
}
