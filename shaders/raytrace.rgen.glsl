#version 460 core
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_debug_printf : enable

layout(binding = 0, set = 0, rgba32f) uniform image2D image;
layout(binding = 1, set = 0) uniform accelerationStructureEXT tlas;

void main()
{
    imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(1.0, 0.0, 0.0, 1.0));
}