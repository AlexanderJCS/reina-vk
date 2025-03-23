#version 460

layout(location = 0) in vec2 uv;
layout(binding = 0, set = 0) uniform sampler2D ldrImage;

layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = texture(ldrImage, uv);
}