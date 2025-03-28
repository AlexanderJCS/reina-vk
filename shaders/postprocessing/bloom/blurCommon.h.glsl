layout(binding = 0, rgba32f) readonly uniform image2D inImage;
layout(binding = 1, rgba32f) writeonly uniform image2D outImage;

#include "bloom.h"

layout (push_constant) uniform PushConsts {
    BloomPushConsts pushConstants;
};

float gauss(float x, float sigma) {
    // non-normalized Gaussian distribution
    return exp(-x * x / (2.0 * sigma * sigma));
}

void blurAxis(ivec2 axis, bool applyThreshold) {
    ivec2 imageSize = imageSize(inImage);
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);

    if (pixelCoord.x >= imageSize.x || pixelCoord.y >= imageSize.y) {
        return;
    }

    const float radiusPx = (axis.x == 1 ? imageSize.x : imageSize.y) * pushConstants.radius / 100.0;
    const int kernelSize = int(radiusPx * 3 + 0.5);

    vec3 color = vec3(0);

    float weightSum = 0;
    for (int i = -kernelSize; i <= kernelSize; i++) {
        float weight = gauss(float(i), pushConstants.radius);
        weightSum += weight;

        ivec2 coord = pixelCoord + i * axis;
        if (coord.x < 0 || coord.x >= imageSize.x || coord.y < 0 || coord.y >= imageSize.y) {
            continue;
        }

        vec4 pixel = imageLoad(inImage, coord);
        float luminosity = dot(pixel.rgb, vec3(0.299, 0.587, 0.114));

        // If threshold is enabled and the pixel is below threshold, skip it entirely.
        if (applyThreshold && luminosity < pushConstants.threshold) {
            continue;
        }

        color += pixel.rgb * weight;
    }

    if (weightSum < 0.0001) {
        color = vec3(0);
    } else {
        color /= weightSum;
    }

    imageStore(outImage, pixelCoord, vec4(color, 1));
}