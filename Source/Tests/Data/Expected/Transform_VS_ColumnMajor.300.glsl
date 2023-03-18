#version 300
#extension GL_ARB_separate_shader_objects : require

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(std140) uniform type_cbVS
{
    layout(row_major) mat4 wvp;
} cbVS;

in vec4 in_var_POSITION;

mat4 spvWorkaroundRowMajor(mat4 wrap) { return wrap; }

void main()
{
    gl_Position = spvWorkaroundRowMajor(cbVS.wvp) * in_var_POSITION;
}

