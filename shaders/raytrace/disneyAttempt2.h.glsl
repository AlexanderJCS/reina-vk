#ifndef REINA_DISNEY_ATTEMPT_2_H
#define REINA_DISNEY_ATTEMPT_2_H

#include "shaderCommon.h.glsl"

// ============================================================================
//                                 Diffuse
// ============================================================================

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

vec3 diffuse(vec3 baseColor, vec3 n, vec3 wi, vec3 wo, vec3 h) {
    // hard-coded for now
    const float roughness = 0.5;
    const float subsurface = 0.5;

    vec3 baseDiffuse = fBaseDiffuse(baseColor, roughness, n, wi, wo, h);
    vec3 fSubsurface = fSubsurface(baseColor, roughness, wi, wo, n, h);

    return mix(baseDiffuse, fSubsurface, subsurface);
}

float pdfDiffuse(vec3 wi, vec3 n) {
    // Lambertian PDF
    float cosTheta = dot(n, wi);

    if (cosTheta <= 0.0) {
        return 0.0;
    }

    return cosTheta * k_inv_pi;
}

vec3 sampleDiffuse(vec3 normal, inout uint rngState) {
    // Cosine hemisphere sampling
    const float theta = 2.0 * k_pi * random(rngState);  // Random in [0, 2pi]
    const float u = 2.0 * random(rngState) - 1.0;   // Random in [-1, 1]
    const float r = sqrt(1.0 - u * u);
    const vec3 direction = normal + vec3(r * cos(theta), r * sin(theta), u);

    return normalize(direction);
}

// ============================================================================
//                                    Metal
// ============================================================================

vec3 evalFm(vec3 baseColor, vec3 h, vec3 wo) {
    return baseColor + (1 - baseColor) * pow(1 - abs(dot(h, wo)), 5);
}

vec3 evalDm(vec3 hl, float alphax, float alphay, float roughness, float aspect, float anisotropic) {
    float constant = (k_pi * alphax * alphay);
    float otherTerm = pow(1 + (pow(hl.x, 2) / pow(alphax, 2) + pow(hl.y, 2) / pow(alphay, 2)), 2);

    return 1 / (constant * otherTerm);
}

float lambda(vec3 wl, float alphax, float alphay) {
    float sqrtTerm = sqrt(1 + (pow(wl.x * alphax, 2) + pow(wl.y * alphay, 2)) / pow(wl.z, 2) - 1);
    return (sqrtTerm - 1) / 2;
}

float smithG(vec3 wl, float alphax, float alphay) {
    // wl is in tangent space
    return 1 / (1 + lambda(wl, alphax, alphay));
}

vec3 evalGm(vec3 wi, vec3 wo, float alphax, float alphay) {
    float g1 = smithG(wi, alphax, alphay);
    float g2 = smithG(wo, alphax, alphay);
    return g1 * g2;
}

vec3 fMetal(mat3 tbn, vec3 baseColor, float anisotropic, float roughness, vec3 n, vec3 wi, vec3 wo, vec3 h) {
    const float alphamin = 0.0001;

    float aspect = sqrt(1 - 0.9 * anisotropic);
    float alphax = max(alphamin, roughness * roughness / aspect);
    float alphay = max(alphamin, roughness * roughness * aspect);

    vec3 fm = evalFm(baseColor, h, wo);
    vec3 dm = evalDm(h, alphax, alphay, roughness, aspect, anisotropic);

    // TODO: confirm that this is correct (i.e., I don't need to multiply by the transpose of the TBN)
    vec3 wiTangent = vec3(tbn * wi);
    vec3 woTangent = vec3(tbn * wo);

    vec3 gm = evalGm(wiTangent, woTangent, alphax, alphay);

    float NdotWi = abs(dot(n, wi));
    return fm * dm * gm / (4 * NdotWi);
}

#endif