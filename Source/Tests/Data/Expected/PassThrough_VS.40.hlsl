static float4 gl_Position;
static float4 in_var_POSITION;
static float2 in_var_TEXCOORD0;
static float2 out_var_TEXCOORD0;

struct SPIRV_Cross_Input
{
    float4 in_var_POSITION : TEXCOORD0;
    float2 in_var_TEXCOORD0 : TEXCOORD1;
};

struct SPIRV_Cross_Output
{
    float2 out_var_TEXCOORD0 : TEXCOORD0;
    float4 gl_Position : SV_Position;
};

void vert_main()
{
    out_var_TEXCOORD0 = in_var_TEXCOORD0;
    gl_Position = in_var_POSITION;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    in_var_POSITION = stage_input.in_var_POSITION;
    in_var_TEXCOORD0 = stage_input.in_var_TEXCOORD0;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.out_var_TEXCOORD0 = out_var_TEXCOORD0;
    return stage_output;
}
