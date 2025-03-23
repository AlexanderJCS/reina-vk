#version 460

// for DEBUG_SHOW_NORMALS
#include "../../../polyglot/common.h"

layout (local_size_x = 32, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0, rgba32f) readonly uniform image2D inImage;
layout(binding = 1, rgba8) writeonly uniform image2D outImage;

vec3 tonemapReinhard(vec3 hdrColor) {
    return hdrColor / (hdrColor + vec3(1.0));
}

vec3 tonemapACES(vec3 color) {
    // https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
    // todo: implement better ACES tonemapping like at https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;

    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), vec3(0), vec3(1));
}

void main() {
    // todo: there's no sRGB conversion, but from my experience that gives poor contrast. so no sRGB for now

    // Get the global pixel coordinate for this invocation
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 imageSize = imageSize(inImage);

    if (pixelCoord.x >= imageSize.x || pixelCoord.y >= imageSize.y) {
        return;
    }

    vec4 pixel = imageLoad(inImage, pixelCoord);
    vec3 color = pixel.rgb;

    #ifndef DEBUG_SHOW_NORMALS
        color = tonemapACES(color);
    #endif

    imageStore(outImage, pixelCoord, vec4(color, 1));
}