#version 460

// for DEBUG_SHOW_NORMALS
#include "../polyglot/common.h"

layout(location = 0) out vec4 fragColor;

layout(binding = 0, set = 0, rgba32f) uniform image2D hdrImage;

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
    vec4 color = imageLoad(hdrImage, ivec2(gl_FragCoord.xy));

    #ifdef DEBUG_SHOW_NORMALS
        vec3 tonemapped = color.xyz;
    #else
        vec3 tonemapped = tonemapACES(color.rgb);
    #endif

    // todo: there's no sRGB conversion, but from my experience that gives poor contrast. so no sRGB for now
    fragColor = vec4(tonemapped, color.w);
}