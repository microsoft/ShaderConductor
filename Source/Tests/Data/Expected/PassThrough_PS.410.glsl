#version 410

layout(location = 0) in vec4 in_var_COLOR;
layout(location = 0) out vec4 out_var_SV_Target;

void main()
{
    out_var_SV_Target = in_var_COLOR;
}

