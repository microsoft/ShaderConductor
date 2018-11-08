#version 300
#extension GL_ARB_separate_shader_objects : require

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    gl_Position = vec4(1.0, 2.0, 3.0, 4.0);
}

