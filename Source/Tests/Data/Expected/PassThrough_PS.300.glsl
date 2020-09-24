#version 300
#extension GL_ARB_separate_shader_objects : require

layout(location = 0) in vec4 in_var_COLOR;
out vec4 out_var_SV_Target;

void main()
{
    out_var_SV_Target = in_var_COLOR;
}

