#version 460

layout(location = 0) out vec4 fragColor;
layout(binding = 0, set = 0, rgba8) uniform image2D ldrImage;

void main() {
    vec4 color = imageLoad(ldrImage, ivec2(gl_FragCoord.xy));
    fragColor = vec4(color);
}