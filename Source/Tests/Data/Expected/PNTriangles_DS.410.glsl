#version 410
layout(triangles) in;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(std140) uniform type_cbPNTriangles
{
    mat4 viewProj;
    vec4 lightDir;
} cbPNTriangles;

layout(location = 2) patch in vec3 in_var_POSITION3;
layout(location = 3) patch in vec3 in_var_POSITION4;
layout(location = 4) patch in vec3 in_var_POSITION5;
layout(location = 5) patch in vec3 in_var_POSITION6;
layout(location = 6) patch in vec3 in_var_POSITION7;
layout(location = 7) patch in vec3 in_var_POSITION8;
layout(location = 0) patch in vec3 in_var_CENTER;
layout(location = 1) in vec3 in_var_POSITION[];
layout(location = 8) in vec2 in_var_TEXCOORD[];
layout(location = 0) out vec2 out_var_TEXCOORD0;

void main()
{
    vec3 _55_unrolled[3];
    for (int i = 0; i < int(3); i++)
    {
        _55_unrolled[i] = in_var_POSITION[i];
    }
    vec2 _56_unrolled[3];
    for (int i = 0; i < int(3); i++)
    {
        _56_unrolled[i] = in_var_TEXCOORD[i];
    }
    float _67 = gl_TessCoord.x * gl_TessCoord.x;
    float _68 = gl_TessCoord.y * gl_TessCoord.y;
    float _69 = gl_TessCoord.z * gl_TessCoord.z;
    float _70 = _67 * 3.0;
    float _71 = _68 * 3.0;
    float _72 = _69 * 3.0;
    gl_Position = cbPNTriangles.viewProj * vec4(((((((((((_55_unrolled[0] * _69) * gl_TessCoord.z) + ((_55_unrolled[1] * _67) * gl_TessCoord.x)) + ((_55_unrolled[2] * _68) * gl_TessCoord.y)) + ((in_var_POSITION3 * _72) * gl_TessCoord.x)) + ((in_var_POSITION4 * gl_TessCoord.z) * _70)) + ((in_var_POSITION8 * _72) * gl_TessCoord.y)) + ((in_var_POSITION5 * _70) * gl_TessCoord.y)) + ((in_var_POSITION7 * gl_TessCoord.z) * _71)) + ((in_var_POSITION6 * gl_TessCoord.x) * _71)) + ((((in_var_CENTER * 6.0) * gl_TessCoord.z) * gl_TessCoord.x) * gl_TessCoord.y), 1.0);
    out_var_TEXCOORD0 = ((_56_unrolled[0] * gl_TessCoord.z) + (_56_unrolled[1] * gl_TessCoord.x)) + (_56_unrolled[2] * gl_TessCoord.y);
}

