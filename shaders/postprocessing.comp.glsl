#version 460

layout (local_size_x = 32, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0, rgba32f) readonly uniform image2D inImage;
layout(binding = 1, rgba8) writeonly uniform image2D outImage;

void main() {
    // Get the global pixel coordinate for this invocation
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 imageSize = imageSize(inImage);

    if (pixelCoord.x >= imageSize.x || pixelCoord.y >= imageSize.y) {
        return;
    }

    vec4 pixel = imageLoad(inImage, pixelCoord);
    imageStore(outImage, pixelCoord, pixel);
}