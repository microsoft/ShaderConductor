#version 300
#extension GL_ARB_separate_shader_objects : require

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(std140) uniform type_cbVS
{
    mat4 wvp;
} cbVS;

in vec4 in_var_POSITION;

void main()
{
    gl_Position = cbVS.wvp * in_var_POSITION;
}

