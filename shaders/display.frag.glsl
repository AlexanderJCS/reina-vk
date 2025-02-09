#version 460

layout(location = 0) out vec4 fragColor;

layout(binding = 0, set = 0, rgba32f) uniform image2D hdrImage;

vec3 reinhard(vec3 hdrColor) {
    return hdrColor / (hdrColor + vec3(1.0));
}

void main() {
    vec4 originalColor = imageLoad(hdrImage, ivec2(gl_FragCoord.xy));

    fragColor = vec4(originalColor.xyz, originalColor.w);
}