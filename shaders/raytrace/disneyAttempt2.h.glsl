#ifndef REINA_DISNEY_ATTEMPT_2_H
#define REINA_DISNEY_ATTEMPT_2_H

#include "shaderCommon.h.glsl"

float FD90(float roughness, vec3 h, vec3 wo) {
    return 0.5 + 2 * roughness * abs(dot(h, wo)) * abs(dot(h, wo));
}

float FD(float roughness, vec3 h, vec3 n, vec3 w) {
    // parameter vec3 w could either be wo or wi
    return 1 + (FD90(roughness, h, w) - 1) * pow(1 - abs(dot(n, w)), 5);
}

float FSS90(float roughness, vec3 h, vec3 wo) {
    return roughness * abs(dot(h, wo)) * abs(dot(h, wo));
}

float FSS(float roughness, vec3 n, vec3 w, vec3 wo, vec3 h) {
    return (1 + (FSS90(roughness, h, wo) - 1) * pow(1 - abs(dot(n, w)), 5));
}

vec3 fSubsurface(vec3 baseColor, float roughness, vec3 wi, vec3 wo, vec3 n, vec3 h) {
    vec3 k = 1.25 * baseColor / k_pi;
    float fssIn = FSS(roughness, n, wi, wo, h);
    float fssOut = FSS(roughness, n, wo, wo, h);

    // Not creatively named, I know
    float thirdTerm = 1 / (abs(dot(n, wi)) + abs(dot(n, wo))) - 0.5;

    return k * (fssIn * fssOut * thirdTerm + 0.5) * abs(dot(n, wo));
}

vec3 fBaseDiffuse(vec3 baseColor, float roughness, vec3 n, vec3 wi, vec3 wo, vec3 h) {
    float NdotL = abs(dot(n, wi));

    float FDin = FD(roughness, h, n, wi);
    float FDout = FD(roughness, h, n, wo);

    return baseColor / k_pi * FDin * FDout * NdotL;
}

vec3 diffuse(vec3 baseColor, vec3 n, vec3 wi, vec3 wo, vec3 h, out float pdf) {
    pdf = 0;

    // hard-coded for now
    const float roughness = 0.5;
    const float subsurface = 0.5;

    vec3 baseDiffuse = fBaseDiffuse(baseColor, roughness, n, wi, wo, h);
    vec3 fSubsurface = fSubsurface(baseColor, roughness, wi, wo, n, h);

    return mix(baseDiffuse, fSubsurface, subsurface);
}

#endif