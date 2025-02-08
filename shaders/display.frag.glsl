#version 460

layout(location = 0) out vec4 fragColor;

layout(binding = 0, set = 0, rgba32f) uniform image2D hdrImage;

void main() {
    fragColor = imageLoad(hdrImage, ivec2(0, 0));
}