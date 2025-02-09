#version 460

layout(location = 0) out vec4 fragColor;

layout(binding = 0, set = 0, rgba32f) uniform image2D hdrImage;

vec3 tonemapReinhard(vec3 hdrColor) {
    return hdrColor / (hdrColor + vec3(1.0));
}

vec3 tonemapACES(vec3 color) {
    color = max(vec3(0.0), color - vec3(0.004));
    color = (color * (6.2 * color + 0.5)) / (color * (6.2 * color + 1.7) + 0.06);
    return color;
}

void main() {
    vec4 color = imageLoad(hdrImage, ivec2(gl_FragCoord.xy));
    vec3 tonemapped = tonemapFilmic(color.rgb);

    // todo: there's no sRGB conversion, but from my experience that gives poor contrast. so no sRGB for now
    fragColor = vec4(tonemapped, color.w);
}