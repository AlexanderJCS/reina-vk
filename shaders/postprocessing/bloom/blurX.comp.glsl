#version 460

layout (local_size_x = 32, local_size_y = 8, local_size_z = 1) in;

#include "blurCommon.h.glsl"

void main() {
    blurAxis(ivec2(1, 0));
}
