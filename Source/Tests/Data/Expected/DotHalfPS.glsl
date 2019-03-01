#version 30
#if defined(GL_AMD_gpu_shader_half_float)
#extension GL_AMD_gpu_shader_half_float : require
#elif defined(GL_NV_gpu_shader5)
#extension GL_NV_gpu_shader5 : require
#else
#error No extension available for FP16.
#endif
#extension GL_ARB_separate_shader_objects : require

void main()
{
    gl_FragData[0] = vec4(float(dot(f16vec3(float16_t(0.0), float16_t(0.0), float16_t(1.0)), f16vec3(float16_t(0.0), float16_t(0.0), float16_t(1.0)))));
}

