#version 300
#extension GL_ARB_separate_shader_objects : require

out gl_PerVertex
{
    vec4 gl_Position;
};

in vec4 in_var_POSITION;
in vec2 in_var_TEXCOORD0;
layout(location = 0) out vec2 varying_TEXCOORD0;

void main()
{
    varying_TEXCOORD0 = in_var_TEXCOORD0;
    gl_Position = in_var_POSITION;
}

