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
    vec4 originalColor = imageLoad(hdrImage, ivec2(gl_FragCoord.xy));
    vec4 tonemapped = vec4(tonemapACES(originalColor.xyz), originalColor.w);

    fragColor = tonemapped;
}