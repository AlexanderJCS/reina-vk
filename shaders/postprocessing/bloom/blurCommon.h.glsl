layout(binding = 0, rgba32f) readonly uniform image2D inImage;
layout(binding = 1, rgba32f) writeonly uniform image2D outImage;

const float M_PI = 3.14159265;

float gauss(float x, float sigma) {
    return (1.0 / (sigma * sqrt(2.0 * M_PI))) * exp(-x * x / (2.0 * sigma * sigma));
}

const int KERNEL_SIZE = 5;

void blurAxis(ivec2 axis) {
    ivec2 imageSize = imageSize(inImage);
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);

    if (pixelCoord.x >= imageSize.x || pixelCoord.y >= imageSize.y || pixelCoord.x < 0 || pixelCoord.y < 0) {
        return;
    }

    vec3 color = vec3(0);
    float weightSum = 0;

    for (int i = -KERNEL_SIZE; i <= KERNEL_SIZE; i++) {
        ivec2 coord = pixelCoord + i * axis;
        if (coord.x < 0 || coord.x >= imageSize.x || coord.y < 0 || coord.y >= imageSize.y) {
            continue;
        }

        vec4 pixel = imageLoad(inImage, pixelCoord + i * axis);
        float weight = gauss(float(i), 3.0);
        color += pixel.rgb * weight;
        weightSum += weight;
    }

    color /= weightSum;
    imageStore(outImage, pixelCoord, vec4(color, 1));
}