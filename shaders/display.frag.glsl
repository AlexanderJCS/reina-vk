#version 460

layout(location = 0) out vec4 fragColor;

layout(binding = 0, set = 0, rgba32f) uniform image2D hdrImage;

void main() {
    ivec2 size = imageSize(hdrImage);
    fragColor = vec4(gl_FragCoord.xy / size, 0, 1);
}