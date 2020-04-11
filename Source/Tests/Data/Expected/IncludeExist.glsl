#version 30
#extension GL_ARB_separate_shader_objects : require

uniform sampler2D SPIRV_Cross_CombinedcolorTexpointSampler;
uniform sampler2D SPIRV_Cross_CombinedcolorTexlinearSampler;

varying vec2 varying_TEXCOORD0;

void main()
{
    gl_FragData[0] = texture2D(SPIRV_Cross_CombinedcolorTexpointSampler, varying_TEXCOORD0) + texture2D(SPIRV_Cross_CombinedcolorTexlinearSampler, varying_TEXCOORD0);
}

