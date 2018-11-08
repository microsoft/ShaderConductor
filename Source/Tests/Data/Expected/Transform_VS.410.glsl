#version 410

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(std140) uniform type_cbVS
{
    layout(row_major) mat4 wvp;
} cbVS;

layout(location = 0) in vec4 in_var_POSITION;

void main()
{
    gl_Position = cbVS.wvp * in_var_POSITION;
}

