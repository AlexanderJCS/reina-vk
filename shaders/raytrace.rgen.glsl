#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

layout(binding = 0, set = 0, rgba32f) uniform image2D image;
layout(binding = 1, set = 0) uniform accelerationStructureEXT tlas;

layout(location = 0) rayPayloadEXT vec3 pld;

void main() {
    const ivec2 resolution = imageSize(image);
    const ivec2 pixel = ivec2(gl_LaunchIDEXT.xy);

    const vec2 screenUV = vec2(
        2.0 * (float(pixel.x) + 0.5 - 0.5 * resolution.x) / resolution.y,
        -(2.0 * (float(pixel.y) + 0.5 - 0.5 * resolution.y) / resolution.y)
    );

    if (pixel.x > resolution.x || pixel.y > resolution.y) {
        return;
    }

    const vec3 cameraOrigin = vec3(0, 0, 0);
    const float fovVerticalSlope = 1.0;
    vec3 rayDirection = normalize(vec3(fovVerticalSlope * screenUV.x, fovVerticalSlope * screenUV.y, -1.0));
    vec3 rayOrigin = vec3(0);

    traceRayEXT(
        tlas,                  // Top-level acceleration structure
        gl_RayFlagsOpaqueEXT,  // Ray flags, here saying "treat all geometry as opaque"
        0xFF,                  // 8-bit instance mask, here saying "trace against all instances"
        0,                     // SBT record offset
        0,                     // SBT record stride for offset
        0,                     // Miss index
        rayOrigin,             // Ray origin
        0,                     // Minimum t-value
        rayDirection,          // Ray direction
        10000.0,               // Maximum t-value
        0                      // Location of payload
    );

    imageStore(image, pixel, vec4(pld, 1));
}