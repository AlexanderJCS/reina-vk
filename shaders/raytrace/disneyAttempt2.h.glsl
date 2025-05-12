#ifndef REINA_DISNEY_ATTEMPT_2_H
#define REINA_DISNEY_ATTEMPT_2_H

#include "shaderCommon.h.glsl"

// ============================================================================
//                                    GGX
// ============================================================================
vec3 sampleGGXVNDF(vec3 V, float ax, float ay, inout uint rngState) {
    float r1 = random(rngState);
    float r2 = random(rngState);

    // https://github.com/knightcrawler25/GLSL-PathTracer/blob/291c1fdc3f97b2a2602c946b41cecca9c3092af7/src/shaders/common/sampling.glsl#L70
    vec3 Vh = normalize(vec3(ax * V.x, ay * V.y, V.z));

    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    vec3 T1 = lensq > 0 ? vec3(-Vh.y, Vh.x, 0) * inversesqrt(lensq) : vec3(1, 0, 0);
    vec3 T2 = cross(Vh, T1);

    float r = sqrt(r1);
    float phi = 2.0 * k_pi * r2;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;

    vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;

    return normalize(vec3(ax * Nh.x, ay * Nh.y, max(0.0, Nh.z)));
}

float D_GGX_Aniso(vec3 m, float alphax, float alphay) {
    float NoM = max(m.z, 0.0);
    float Tm  = m.x;
    float Bm  = m.y;
    float inv = 1.0 / ((Tm / alphax) * (Tm / alphax) + (Bm / alphay) * (Bm / alphay) + NoM * NoM);
    return inv * inv / (k_pi * alphax * alphay);
}

float D(vec3 m, vec2 alpha) {
    // anisotropic form:
    return D_GGX_Aniso(m, alpha.x, alpha.y);
}

float pdfGGXReflection(vec3 i, vec3 o, vec2 alpha) {
    // TODO: chatgpt said there's something wrong with this:
    //  Your pdfGGXReflection attempts a more complex “anisotropic” form but is not algebraically equivalent to the
    //  standard Jacobian‐corrected VNDF PDF above. In particular, it omits the G1(v)G1(v) term in the numerator and the
    //  factor of 4(v⋅m)4(v⋅m) in the denominator, instead folding them into an alternate empirical expression.

    // https://dl.acm.org/doi/10.1145/3610543.3626163

    vec3 m = normalize(i + o);
    float ndf = D(m, alpha );
    vec2 ai = alpha * i.xy ;
    float len2 = dot(ai, ai);
    float t = sqrt(len2 + i.z * i.z);
    if (i.z >= 0.0) {
        float a = clamp(min(alpha .x, alpha.y), 0, 1); // Eq . 6
        float s = 1.0 + length(vec2(i.x, i.y)); // Omit sgn for a <=1
        float a2 = a * a; float s2 = s * s;
        float k = (1.0 - a2 ) * s2 / ( s2 + a2 * i.z * i.z); // Eq . 5
        return ndf / (2.0 * (k * i.z + t)); // Eq . 8 * || dm / do ||
    }

    // Numerically stable form of the previous PDF for i.z < 0
    return ndf * (t - i.z) / (2.0 * len2 ); // = Eq . 7 * || dm / do ||
}


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

vec3 diffuse(float roughness, float subsurface, vec3 baseColor, vec3 n, vec3 wi, vec3 wo, vec3 h) {
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

float evalDm(vec3 hl, float alphax, float alphay, float roughness, float aspect, float anisotropic) {
    float constant = (k_pi * alphax * alphay);
    float otherTerm = pow(pow(hl.x, 2) / pow(alphax, 2) + pow(hl.y, 2) / pow(alphay, 2) + pow(hl.z, 2), 2);

    return 1 / (constant * otherTerm);
}

float lambda(vec3 wl, float alphax, float alphay) {
    float sqrtTerm = sqrt(1 + (pow(wl.x * alphax, 2) + pow(wl.y * alphay, 2)) / pow(wl.z, 2));
    return (sqrtTerm - 1) / 2;
}

float smithG(vec3 wl, float alphax, float alphay) {
    // wl is in tangent space
    return 1 / (1 + lambda(wl, alphax, alphay));
}

float evalGm(vec3 wi, vec3 wo, float alphax, float alphay) {
    float g1 = smithG(wi, alphax, alphay);
    float g2 = smithG(wo, alphax, alphay);
    return g1 * g2;
}

vec3 metal(mat3 tbn, vec3 baseColor, float anisotropic, float roughness, vec3 n, vec3 wi, vec3 wo, vec3 h) {
    const float alphamin = 0.0001;

    float aspect = sqrt(1 - 0.9 * anisotropic);
    float alphax = max(alphamin, roughness * roughness / aspect);
    float alphay = max(alphamin, roughness * roughness * aspect);

    vec3 fm = evalFm(baseColor, h, wo);

    vec3 wiTangent = vec3(transpose(tbn) * wi);
    vec3 woTangent = vec3(transpose(tbn) * wo);
    vec3 hTangent = vec3(transpose(tbn) * h);

    float dm = evalDm(hTangent, alphax, alphay, roughness, aspect, anisotropic);
    float gm = evalGm(wiTangent, woTangent, alphax, alphay);

    float NdotWi = abs(dot(n, wi));
    return fm * dm * gm / (4 * NdotWi);
}

vec3 sampleMetal(mat3 tbn, vec3 baseColor, float anisotropic, float roughness, vec3 n, vec3 wi, inout uint rngState) {
    // TODO: reused computation between sampling and evaluation should be eliminated
    const float alphamin = 0.0001;

    float aspect = float(sqrt(1 - 0.9 * anisotropic));
    float alphax = max(alphamin, roughness * roughness / aspect);
    float alphay = max(alphamin, roughness * roughness * aspect);

    vec3 wiTangent = vec3(transpose(tbn) * wi);
    vec3 h = sampleGGXVNDF(wiTangent, alphax, alphay, rngState);

    // transform h back to world space
    h = normalize(vec3(tbn * h));
    vec3 wo = reflect(-wi, h);

    return wo;
}

float pdfMetal(
    mat3 tbn,           // columns = T, B, N
    vec3 wi_world,      // incoming dir (pointing toward surface), |wi_world|=1
    vec3 wo_world,      // outgoing/reflected dir, |wo_world|=1
    float anisotropic,  // Disney anisotropy [0,1]
    float roughness     // Disney roughness [0,1]
) {
    // 1) build anisotropic alpha’s (same as in your sampler)
    const float alpha_min = 1e-4;
    float aspect = sqrt(1.0 - 0.9 * anisotropic);
    float alphax = max(alpha_min, roughness*roughness / aspect);
    float alphay = max(alpha_min, roughness*roughness * aspect);
    
    // 2) transform both wi, wo into tangent‐space
    vec3 wiTangent = vec3(transpose(tbn) * wi_world);
    vec3 woTangent = vec3(transpose(tbn) * wo_world);
    
    // 3) if either is below the hemisphere, PDF = 0
    if (wiTangent.z <= 0.0 || woTangent.z <= 0.0) {
        return 0.0;
    }

    // 4) pack alpha as vec2 for anisotropic D
    vec2 alpha = vec2(alphax, alphay);

    return pdfGGXReflection(wiTangent, woTangent, alpha);
}

#endif