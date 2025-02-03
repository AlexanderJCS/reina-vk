#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 pld;

void main() {
    // Vertical gradient from horizon (dark) to zenith (blue)
    float verticalBlend = gl_WorldRayDirectionEXT.y * 0.5 + 0.5;
    vec3 horizonColor = vec3(0.1, 0.1, 0.1); // Dark gray
    vec3 zenithColor = vec3(0.2, 0.4, 0.9);  // Sky blue
    pld = mix(horizonColor, zenithColor, verticalBlend);
    pld = normalize(gl_WorldRayDirectionEXT) * 0.5 + 0.5;
}