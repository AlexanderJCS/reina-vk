layout(binding = 0, rgba32f) readonly uniform image2D inImage;
layout(binding = 1, rgba32f) writeonly uniform image2D outImage;

const float M_PI = 3.14159265;

float gauss(float x, float sigma) {
    return (1.0 / (sigma * sqrt(2.0 * M_PI))) * exp(-x * x / (2.0 * sigma * sigma));
}

const int KERNEL_SIZE = 25;

void blurAxis(ivec2 axis, bool threshold) {
    ivec2 imageSize = imageSize(inImage);
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);

    if (pixelCoord.x >= imageSize.x || pixelCoord.y >= imageSize.y) {
        return;
    }

    vec3 color = vec3(0);

    for (int i = -KERNEL_SIZE; i <= KERNEL_SIZE; i++) {
        ivec2 coord = pixelCoord + i * axis;
        if (coord.x < 0 || coord.x >= imageSize.x || coord.y < 0 || coord.y >= imageSize.y) {
            continue;
        }

        vec4 pixel = imageLoad(inImage, coord);
        float luminosity = dot(pixel.rgb, vec3(0.299, 0.587, 0.114));

        // If threshold is enabled and the pixel is below threshold, skip it entirely.
        if (threshold && luminosity < 1.0) {
            continue;
        }

        float weight = gauss(float(i), 12.0);
        color += pixel.rgb * weight;
    }

    imageStore(outImage, pixelCoord, vec4(color, 1));
}