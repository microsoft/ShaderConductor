#version 410

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) in vec4 in_var_POSITION;
layout(location = 1) in vec2 in_var_TEXCOORD0;
layout(location = 0) out vec2 out_var_TEXCOORD0;

void main()
{
    out_var_TEXCOORD0 = in_var_TEXCOORD0;
    gl_Position = in_var_POSITION;
}

