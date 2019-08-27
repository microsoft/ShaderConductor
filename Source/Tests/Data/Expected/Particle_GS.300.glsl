#version 300
#extension GL_ARB_separate_shader_objects : require
layout(points) in;
layout(max_vertices = 4, triangle_strip) out;

out gl_PerVertex
{
    vec4 gl_Position;
};

const vec3 _43[4] = vec3[](vec3(-1.0, 1.0, 0.0), vec3(1.0, 1.0, 0.0), vec3(-1.0, -1.0, 0.0), vec3(1.0, -1.0, 0.0));
const vec2 _48[4] = vec2[](vec2(0.0, 1.0), vec2(1.0), vec2(0.0), vec2(1.0, 0.0));

layout(std140) uniform type_cbMain
{
    layout(row_major) mat4 invView;
    layout(row_major) mat4 viewProj;
} cbMain;

layout(location = 0) in vec4 in_var_POSITION[1];
layout(location = 0) out vec2 out_var_TEXCOORD0;

void main()
{
    for (int _55 = 0; _55 < 4; )
    {
        gl_Position = cbMain.viewProj * vec4((mat3(cbMain.invView[0].xyz, cbMain.invView[1].xyz, cbMain.invView[2].xyz) * (_43[_55] * 5.0)) + in_var_POSITION[0].xyz, 1.0);
        out_var_TEXCOORD0 = _48[_55];
        EmitVertex();
        _55++;
        continue;
    }
    EndPrimitive();
}

