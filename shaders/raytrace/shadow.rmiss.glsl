#version 460
#extension GL_EXT_ray_tracing : require

struct ShadowPayload {
    bool occluded;
};

layout(location = 1) rayPayloadInEXT ShadowPayload pld;

void main() {
    pld.occluded = false;
}