#version 410
layout(points) in;
layout(max_vertices = 4, triangle_strip) out;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(std140) uniform type_cbMain
{
    mat4 invView;
    mat4 viewProj;
} cbMain;

layout(location = 0) in vec4 in_var_POSITION[1];
layout(location = 0) out vec2 out_var_TEXCOORD0;

void main()
{
    mat3 _49 = mat3(cbMain.invView[0].xyz, cbMain.invView[1].xyz, cbMain.invView[2].xyz);
    gl_Position = cbMain.viewProj * vec4((_49 * vec3(-5.0, 5.0, 0.0)) + in_var_POSITION[0].xyz, 1.0);
    out_var_TEXCOORD0 = vec2(0.0, 1.0);
    EmitVertex();
    gl_Position = cbMain.viewProj * vec4((_49 * vec3(5.0, 5.0, 0.0)) + in_var_POSITION[0].xyz, 1.0);
    out_var_TEXCOORD0 = vec2(1.0);
    EmitVertex();
    gl_Position = cbMain.viewProj * vec4((_49 * vec3(-5.0, -5.0, 0.0)) + in_var_POSITION[0].xyz, 1.0);
    out_var_TEXCOORD0 = vec2(0.0);
    EmitVertex();
    gl_Position = cbMain.viewProj * vec4((_49 * vec3(5.0, -5.0, 0.0)) + in_var_POSITION[0].xyz, 1.0);
    out_var_TEXCOORD0 = vec2(1.0, 0.0);
    EmitVertex();
    EndPrimitive();
}

