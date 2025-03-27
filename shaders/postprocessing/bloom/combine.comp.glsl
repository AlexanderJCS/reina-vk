#version 460

#include "../../../polyglot/bloom.h"

layout (local_size_x = 32, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0, rgba32f) readonly uniform image2D originalImage;
layout(binding = 1, rgba32f) readonly uniform image2D bloomImage;
layout(binding = 2, rgba32f) writeonly uniform image2D resultImage;

layout (push_constant) uniform PushConsts {
    BloomPushConsts pushConstants;
};

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

    if (pixel.x >= imageSize(resultImage).x || pixel.y >= imageSize(resultImage).y) {
        return;
    }

    vec4 original = imageLoad(originalImage, pixel);
    vec4 bloom = imageLoad(bloomImage, pixel);

    vec3 result = original.rgb + bloom.rgb * pushConstants.intensity;
    imageStore(resultImage, pixel, vec4(result, 1));
}