#version 460

// include common.h for the DEBUG_SHOW_NORMALS definition
#include "raytrace.h"
#include "tonemapping.h"

layout (push_constant) uniform PushConsts {
    TonemappingPushConsts pushConstants;
};

layout (local_size_x = 32, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0, rgba32f) readonly uniform image2D inImage;
layout(binding = 1, rgba8) writeonly uniform image2D outImage;

// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT from: https://github.com/knightcrawler25/GLSL-PathTracer/blob/master/src/shaders/tonemap.glsl
mat3 ACESInputMat = mat3(
    vec3(0.59719, 0.35458, 0.04823),
    vec3(0.07600, 0.90834, 0.01566),
    vec3(0.02840, 0.13383, 0.83777)
);

// ODT_SAT => XYZ => D60_2_D65 => sRGB from: https://github.com/knightcrawler25/GLSL-PathTracer/blob/master/src/shaders/tonemap.glsl
mat3 ACESOutputMat = mat3(
    vec3(1.60475, -0.53108, -0.07367),
    vec3(-0.10208, 1.10813, -0.00605),
    vec3(-0.00327, -0.07276, 1.07602)
);

vec3 tonemapReinhard(vec3 hdrColor) {
    return hdrColor / (hdrColor + vec3(1.0));
}

vec3 RRTAndODTFit(vec3 v) {
    // from: https://github.com/knightcrawler25/GLSL-PathTracer/blob/master/src/shaders/tonemap.glsl
    vec3 a = v * (v + 0.0245786f) - 0.000090537f;
    vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

vec3 tonemapACESFitted(vec3 color) {
    // from: https://github.com/knightcrawler25/GLSL-PathTracer/blob/master/src/shaders/tonemap.glsl
    color = color * ACESInputMat;
    color = RRTAndODTFit(color);  // Apply RRT and ODT
    color = color * ACESOutputMat;

    return clamp(color, 0.0, 1.0);
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

vec3 adjustExposure(vec3 pixel, float exosure) {
    return pixel * exp2(exosure);
}

void main() {
    // Get the global pixel coordinate for this invocation
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 imageSize = imageSize(inImage);

    if (pixelCoord.x >= imageSize.x || pixelCoord.y >= imageSize.y) {
        return;
    }

    vec4 pixel = imageLoad(inImage, pixelCoord);
    vec3 color = pixel.rgb;

    #ifndef DEBUG_SHOW_NORMALS
//        color = adjustExposure(color, pushConstants.exposure);
//        color = tonemapACESFitted(color);
    #endif

    imageStore(outImage, pixelCoord, vec4(color, 1));
}