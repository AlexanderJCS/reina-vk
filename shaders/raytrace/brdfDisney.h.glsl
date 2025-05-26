#ifndef REINA_DISNEY_ATTEMPT_2_H
#define REINA_DISNEY_ATTEMPT_2_H

#include "shaderCommon.h.glsl"

// ============================================================================
//                                    GGX
// ============================================================================
vec3 sampleGGXVNDF(vec3 V, float ax, float ay, inout uint rngState) {
    bool flip = V.z < 0.0;

    if (flip) {
        V.z *= -1;
    }

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

    if (flip) {
        Nh.z *= -1;
    }

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
    return ndf * (t - i.z) / (2.0 * len2); // = Eq . 7 * || dm / do ||
}

// ============================================================================
//                                     GTR
// ============================================================================
vec3 sampleGTR1(float rgh, inout uint rngState) {
    float r1 = random(rngState);
    float r2 = random(rngState);

    float a = max(0.001, rgh);
    float a2 = a * a;

    float phi = r1 * k_pi * 2;

    float tan2Theta = a2 * r2 / (1.0 - r2);
    float cosTheta = 1.0 / sqrt(1.0 + tan2Theta);
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta*cosTheta));
    float sinPhi = sin(phi);
    float cosPhi = cos(phi);

    return vec3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
}

vec3 sampleGTR1_attempt2(float alpha, inout uint rngState) {
    float r1 = random(rngState);
    float r2 = random(rngState);

    float a = max(0.001, alpha);

    float coshElevation = sqrt((1 - exp(r1*log(a*a))) / (1 - a*a));
    float hElevation = acos(coshElevation);
    float hAzimuth = 2.0 * k_pi * r2;

    return vec3(
        sin(hElevation) * cos(hAzimuth),
        sin(hElevation) * sin(hAzimuth),
        coshElevation
    );
}

float pdfGTR1(float roughness, vec3 h) {
    float numerator = roughness * roughness - 1;
    float denominator = k_pi * log(roughness * roughness) * (1 + (roughness * roughness - 1) * (h.z * h.z));

    return numerator / denominator;
}

// ============================================================================
//                                 Diffuse
// ============================================================================

float FD90(float roughness, float HdotWo) {
    return 0.5 + 2 * roughness * max(HdotWo, 0) * max(HdotWo, 0);
}

float FD(float roughness, vec3 h, vec3 n, vec3 w, float fd90) {
    // parameter vec3 w could either be wo or wi
    return 1 + (fd90 - 1) * pow(1 - max(dot(n, w), 0), 5);
}

float FSS90(float roughness, vec3 h, vec3 wo) {
    return roughness * max(dot(h, wo), 0) * max(dot(h, wo), 0);
}

float FSS(float roughness, vec3 n, vec3 w, float fss90) {
    return 1 + (fss90 - 1) * pow(1 - max(dot(n, w), 0), 5);
}

vec3 fSubsurface(vec3 baseColor, float roughness, vec3 wi, vec3 wo, vec3 n, vec3 h) {
    vec3 k = 1.25 * baseColor * k_inv_pi;

    float fss90 = FSS90(roughness, h, wo);
    float fssIn = FSS(roughness, n, wi, fss90);
    float fssOut = FSS(roughness, n, wo, fss90);

    // Not creatively named, I know
    float thirdTerm = 1 / (max(dot(n, wi), 0) + max(dot(n, wo), 0)) - 0.5;

    return k * (fssIn * fssOut * thirdTerm + 0.5);
}

vec3 fBaseDiffuse(vec3 baseColor, float roughness, vec3 n, vec3 wi, vec3 wo, vec3 h) {
    float fd90 = FD90(roughness, dot(h, wo));

    float FDin = FD(roughness, h, n, wi, fd90);
    float FDout = FD(roughness, h, n, wo, fd90);

    return baseColor / k_pi * FDin * FDout;
}

vec3 evalDiffuse(float roughness, float subsurface, vec3 baseColor, vec3 n, vec3 wi, vec3 wo, vec3 h) {
    vec3 baseDiffuse = fBaseDiffuse(baseColor, roughness, n, wi, wo, h);
    vec3 fSubsurface = fSubsurface(baseColor, roughness, wi, wo, n, h);

    return mix(baseDiffuse, fSubsurface, subsurface);
}

float pdfDiffuse(vec3 wo, vec3 n) {
    // Lambertian PDF
    float cosTheta = dot(n, wo);

    if (cosTheta <= 0.0) {
        return 0.0;
    }

    return cosTheta * k_inv_pi;
}

vec3 sampleDiffuse(vec3 n, inout uint rngState) {
    // 1) draw two uniform randoms
    float xi1 = random(rngState);
    float xi2 = random(rngState);

    // 2) convert to spherical coords
    float r   = sqrt(xi1);
    float phi = 2.0 * k_pi * xi2;
    float x   = r * cos(phi);
    float y   = r * sin(phi);
    float z   = sqrt(max(0.0, 1.0 - xi1));  // cosθ

    // 3) build an orthonormal basis (n, t, b)
    vec3 t = abs(n.x) < 0.5
    ? normalize(cross(n, vec3(1,0,0)))
    : normalize(cross(n, vec3(0,1,0)));
    vec3 b = cross(n, t);

    // 4) transform from local→world
    return normalize(x*t + y*b + z*n);
}

// ============================================================================
//                                    Metal
// ============================================================================

float evalR0(float ior) {
    return (ior - 1) * (ior - 1) / ((ior + 1) * (ior + 1));
}

float luminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

vec3 evalFm(vec3 baseColor, vec3 h, vec3 wo, float specular, vec3 specularTint, float metallic, float eta) {
    float lum = luminance(baseColor);
    vec3 ctint = lum > 0.0 ? baseColor / lum : vec3(1);
    vec3 ks = (1 - ctint) + specularTint * ctint;
    vec3 c0 = specular * evalR0(eta) * (1 - metallic) * ks + metallic * baseColor;

    return c0 + (1 - c0) * pow(1 - abs(dot(h, wo)), 5);
}

float evalDm(vec3 hl, float alphax, float alphay) {
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

vec3 evalMetal(mat3 tbn, vec3 baseColor, float anisotropic, float roughness, vec3 n, vec3 wi, vec3 wo, vec3 h, float specular, vec3 specularTint, float metallic, float eta) {
    const float alphamin = 0.0001;

    float aspect = sqrt(1.0 - 0.9 * anisotropic);
    float alphax = max(alphamin, roughness * roughness / aspect);
    float alphay = max(alphamin, roughness * roughness * aspect);

    vec3 fm = evalFm(baseColor, h, wo, specular, specularTint, metallic, eta);

    vec3 wiTangent = vec3(transpose(tbn) * wi);
    vec3 woTangent = vec3(transpose(tbn) * wo);
    vec3 hTangent = vec3(transpose(tbn) * h);

    float dm = evalDm(hTangent, alphax, alphay);
    float gm = evalGm(wiTangent, woTangent, alphax, alphay);

    float NdotWi = abs(dot(n, wi));
    float NdotWo = abs(dot(n, wo));

    // TODO: check if I should be multiplying by NdotWo
    return fm * dm * gm / (4.0 * NdotWi * NdotWo);
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

float pdfMetal(mat3 tbn, vec3 wi_world, vec3 wo_world, float anisotropic, float roughness) {
    const float alpha_min = 1e-4;
    float aspect = sqrt(1.0 - 0.9 * anisotropic);
    float alphax = max(alpha_min, roughness*roughness / aspect);
    float alphay = max(alpha_min, roughness*roughness * aspect);

    vec3 wiTangent = vec3(transpose(tbn) * wi_world);
    vec3 woTangent = vec3(transpose(tbn) * wo_world);

    vec2 alpha = vec2(alphax, alphay);

    return pdfGGXReflection(wiTangent, woTangent, alpha);
}

// ============================================================================
//                                 Clearcoat
// ============================================================================

float lambdac(vec3 wl) {
    float sqrtTerm = sqrt(
        1.0 + (pow(wl.x * 0.25, 2) + pow(wl.y * 0.25, 2)) / pow(wl.z, 2)
    );

    return (sqrtTerm - 1) / 2;
}

float separableSmithGGXG1(vec3 w, float a) {
    // https://schuttejoe.github.io/post/disneybsdf/
    float a2 = a * a;
    float absDotNV = w.z;

    return 2.0f / (1.0f + sqrt(a2 + (1 - a2) * absDotNV * absDotNV));
}

float evalGc(vec3 wiTangent, vec3 woTangent) {
    float gcwo = separableSmithGGXG1(wiTangent, 0.25);
    float gcwi = separableSmithGGXG1(woTangent, 0.25);

    return gcwo * gcwi;
}

float evalDc(float alphag, vec3 hl) {
    float numerator = alphag * alphag - 1;
    float denominator = k_pi * log(alphag * alphag) * (1 + (alphag * alphag - 1) * (hl.z * hl.z));

    return numerator / denominator;
}

float evalFc(vec3 h, vec3 wo) {
    float r0ior = evalR0(1.5);

    return r0ior + (1 - r0ior) * pow(1 - dot(h, wo), 5);
}

vec3 evalClearcoat(mat3 tbn, vec3 wi, vec3 wo, float clearcoatGloss, vec3 h) {
    float alphag = (1 - clearcoatGloss) * 0.1 + clearcoatGloss * 0.001;

    vec3 hTangent = vec3(transpose(tbn) * h);
    vec3 wiTangent = vec3(transpose(tbn) * wi);
    vec3 woTangent = vec3(transpose(tbn) * wo);

    float fc = evalFc(h, wo);
    float gc = evalGc(wiTangent, woTangent);
//    float gc = smithG(wiTangent.z, 0.25) * smithG(woTangent.z, 0.25);
    float dc = evalDc(alphag, hTangent);

    return vec3(0.25 * fc * gc * dc);
}

vec3 sampleClearcoat(mat3 tbn, float clearcoatGloss, vec3 wi, inout uint rngState) {
    float alphag = (1 - clearcoatGloss) * 0.1 + clearcoatGloss * 0.001;

    vec3 h = normalize(sampleGTR1_attempt2(alphag, rngState));

    // transform h to world space
    h = normalize(vec3(tbn * h));
    vec3 wo = normalize(reflect(-wi, h));

    return wo;
}

float pdfClearcoat(mat3 tbn, vec3 wi, vec3 wo, vec3 h, float clearcoatGloss) {
    float alphag = (1 - clearcoatGloss) * 0.1 + clearcoatGloss * 0.001;

    vec3 wiTangent = vec3(transpose(tbn) * wi);
    vec3 woTangent = vec3(transpose(tbn) * wo);
    vec3 hTangent = vec3(transpose(tbn) * h);

    if (wiTangent.z <= 0.0 || woTangent.z <= 0.0) {
        return 0.0;
    }

    float dc = evalDc(alphag, hTangent) * hTangent.z;  // h.z = n dot h

    // p(wo) = p(h) / (4 * wo dot h)
    return dc / (4.0 * abs(dot(woTangent, hTangent)));
}

// ============================================================================
//                                  Glass
// ============================================================================

float reflectance(float cosine, float ri) {
    // Use Schlick's approximation for reflectance
    float r0 = (1 - ri) / (1 + ri);
    r0 = r0 * r0;
    return r0 + (1 - r0) * pow((1 - cosine), 5);
}

vec3 sampleGlass(
    mat3 tbn,
    vec3 wi,
    float roughness,
    float anisotropic,
    float eta,
    inout uint rngState,
    out bool refracted
) {
    // todo: computing alpha is repeated code
    const float alpha_min = 1e-4;
    float aspect = sqrt(1.0 - 0.9 * anisotropic);
    float alphax = max(alpha_min, roughness*roughness / aspect);
    float alphay = max(alpha_min, roughness*roughness * aspect);

    vec3 wiTangent = vec3(transpose(tbn) * wi);
    vec3 hTangent = sampleGGXVNDF(wiTangent, alphax, alphay, rngState);

    float cosTheta = dot(wiTangent, hTangent);
    vec3 hWorld = normalize(vec3(tbn * hTangent));

    float reflectivity = reflectance(cosTheta, eta);

    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    bool cannotRefract = bool(eta * sinTheta > 1.0);

    if (cannotRefract || reflectivity > random(rngState)) {
        // Reflect
        refracted = false;
        return reflect(-wi, hWorld);
    }

    refracted = true;
    // Refract
    return refract(-wi, hWorld, eta);
}

float SmithGAniso(float NDotV, float VDotX, float VDotY, float ax, float ay) {
    // https://github.com/knightcrawler25/GLSL-PathTracer/blob/291c1fdc3f97b2a2602c946b41cecca9c3092af7/src/shaders/common/sampling.glsl#L116-L122
    float a = VDotX * ax;
    float b = VDotY * ay;
    float c = NDotV;
    return (2.0 * NDotV) / (NDotV + sqrt(a * a + b * b + c * c));
}

vec3 evalMicrofacetRefraction(vec3 baseColor, float anisotropic, float roughness, float eta, vec3 V, vec3 L, vec3 H, out float pdf) {
    const float alpha_min = 1e-4;
    float aspect = sqrt(1.0 - 0.9 * anisotropic);
    float alphax = max(alpha_min, roughness*roughness / aspect);
    float alphay = max(alpha_min, roughness*roughness * aspect);

    pdf = 0.0;
    if (L.z >= 0.0) {
        return vec3(0.0);
    }

    float LDotH = dot(L, H);
    float VDotH = dot(V, H);

    float D = evalDm(H, alphax, alphay);
    float G1 = SmithGAniso(abs(V.z), V.x, V.y, alphax, alphay);
    float G2 = G1 * SmithGAniso(abs(L.z), L.x, L.y, alphax, alphay);
    float denom = LDotH + VDotH * eta;
    denom *= denom;
    float eta2 = eta * eta;
    float jacobian = abs(LDotH) * eta2 / denom;

    pdf = G1 * max(0.0, VDotH) * D * jacobian / V.z;

    vec3 F = vec3(reflectance(dot(V, H), eta));
    return pow(baseColor, vec3(0.5)) * (1.0 - F) * D * G2 * abs(VDotH) * jacobian / abs(L.z * V.z);
}

float pdfGlassTransmission(mat3 tbn,
                           vec3 wi_world,
                           vec3 wo_world,
                           float anisotropic,
                           float roughness,
                           float eta) {

    vec3 wiTangent = transpose(tbn) * wi_world;
    vec3 woTangent = transpose(tbn) * wo_world;

    // Reject non‑transmission geometry
    if (wiTangent.z <= 0.0 || woTangent.z >= 0.0)
        return 0.0;

    const float alphamin = 1e-4;
    float aspect = sqrt(1.0 - 0.9 * anisotropic);
    float alphax = max(alphamin, roughness*roughness / aspect);
    float alphay = max(alphamin, roughness*roughness * aspect);
    vec2  alpha  = vec2(alphax, alphay);

    vec3 m = normalize(wiTangent + eta * woTangent);

    // PDF of visible normal
    float D    = D_GGX_Aniso(m, alphax, alphay);
    float G1   = SmithGAniso(abs(wiTangent.z), wiTangent.x, wiTangent.y, alphax, alphay);
    float dotWiM = abs(dot(wiTangent, m));
    float p_m = D * G1 * dotWiM / abs(wiTangent.z);

    // Jacobian
    float dotWoM = dot(woTangent, m);
    float jacobian = eta*eta * dotWiM / pow(dotWiM + eta * dotWoM, 2);

    return p_m * jacobian;
}

vec3 glassTransmission(mat3 tbn, vec3 baseColor, vec3 wo, vec3 h, vec3 wi, float roughness, float anisotropic, float eta) {
    const float alpha_min = 1e-4;
    float aspect = sqrt(1.0 - 0.9 * anisotropic);
    float ax = max(alpha_min, roughness*roughness / aspect);
    float ay = max(alpha_min, roughness*roughness * aspect);

    // https://schuttejoe.github.io/post/disneybsdf/
    vec3 wiTangent = vec3(transpose(tbn) * wi);
    vec3 woTangent = vec3(transpose(tbn) * wo);
    vec3 hTangent = vec3(transpose(tbn) * h);

    float relativeIor = eta;
    float n2 = relativeIor * relativeIor;

    float absDotNL = abs(wiTangent.z);
    float absDotNV = abs(woTangent.z);
    float dotHL = dot(hTangent, wiTangent);
    float dotHV = dot(hTangent, woTangent);
    float absDotHL = abs(dotHL);
    float absDotHV = abs(dotHV);

    float d = evalDm(hTangent, ax, ay);
    float gl = evalGm(wiTangent, hTangent, ax, ay);
    float gv = evalGm(woTangent, hTangent, ax, ay);

    float f = reflectance(dotHV, eta);

    vec3 color = baseColor;
    float spreading = 1.0 / (eta * eta);

    float c = (absDotHL * absDotHV) / (absDotNL * absDotNV);
    float t = (n2 / pow(dotHL + relativeIor * dotHV, 2));
    return spreading * color * c * t * (1.0f - f) * gl * gv * d;
}

vec3 glass(mat3 tbn, vec3 baseColor, float anisotropic, float roughness, float eta, vec3 n, vec3 wi, vec3 wo, bool didRefract, out float pdf) {
    vec3 h;
    if (didRefract) {
        h = normalize(wo + wi * eta);
    } else {
        h = normalize(wo + wi);
    }

    if (dot(h, n) < 0.0) {
        h = -h;
    }

    float transmissionpdf;
    vec3 glassf = evalMicrofacetRefraction(
        baseColor,
        anisotropic,
        roughness,
        eta,
        transpose(tbn) * wi,
        transpose(tbn) * wo,
        transpose(tbn) * h,
        transmissionpdf
    );

    // add metal component
    // metal(mat3 tbn, vec3 baseColor, float anisotropic, float roughness, vec3 n, vec3 wi, vec3 wo, vec3 h, float specular, vec3 specularTint, float metallic, float eta)
    float metalpdf = pdfMetal(tbn, wi, wo, anisotropic, roughness);
    vec3 metalf = evalMetal(tbn, baseColor, anisotropic, roughness, n, wi, wo, h, 0.0, vec3(1.0), 1.0, eta);

//    float cosTheta = dot(h, n);
//    float reflectivity = reflectance(cosTheta, eta);

    // I can't justify this mathematically but it seems to provide good results. the PDF should theoretically be
    //  transmissionpdf * (1 - reflectivity) and metalpdf * reflectivity, but not including the reflectivity multiply
    //  seems to make the glass look better (i.e., not lose energy as fast when albedo = 1).
    if (didRefract) {
        pdf = transmissionpdf;
        return glassf;
    }

    pdf = metalpdf;
    return metalf;
}

// ============================================================================
//                                  Sheen
// ============================================================================

vec3 evalSheen(vec3 baseColor, vec3 wo, vec3 h, vec3 n, vec3 sheenTint) {
    float lum = luminance(baseColor);
    vec3 ctint = lum > 0.0 ? baseColor / lum : vec3(1);
    vec3 csheen = mix(vec3(1.0), ctint, sheenTint);

    vec3 fsheen = csheen * pow(1 - max(dot(h, wo), 0), 5) * max(dot(n, wo), 0);
    return fsheen;
}

vec3 sampleSheen(vec3 n, inout uint rngState) {
    return sampleDiffuse(n, rngState);
}

float pdfSheen(vec3 n, vec3 wo) {
    // Lambertian reflection
    return max(dot(n, wo), 0.0) * k_inv_pi;
}

// ============================================================================
//                            Putting it Together
// ============================================================================

vec3 sampleDisney(
    mat3 tbn,
    vec3 baseColor,
    float anisotropic,
    float roughness,
    float clearcoatGloss,
    float eta,
    float metallic,
    float clearcoat,
    float specTransmission,
    vec3 n,
    vec3 wi,
    out bool didRefract,
    inout uint rngState
) {
    float diffuseWt = (1 - specTransmission) * (1 - metallic);
    float metalWt = metallic;
    float clearcoatWt = 0.25 * clearcoat;
    float glassWt = (1 - metallic) * specTransmission;

    float cdf[4];
    cdf[0] = diffuseWt;
    cdf[1] = cdf[0] + metalWt;
    cdf[2] = cdf[1] + clearcoatWt;
    cdf[3] = cdf[2] + glassWt;

    float rand = random(rngState) * cdf[3];
    if (rand < cdf[0]) {
        // Diffuse
        return sampleDiffuse(n, rngState);
    } else if (rand < cdf[1]) {
        // Metal
        return sampleMetal(tbn, baseColor, anisotropic, roughness, n, wi, rngState);
    } else if (rand < cdf[2]) {
        // Clearcoat
        return sampleClearcoat(tbn, clearcoatGloss, wi, rngState);
    }

    // Glass
    return sampleGlass(tbn, wi, roughness, anisotropic, eta, rngState, didRefract);
}

vec3 evalDisney(
    mat3 tbn,
    vec3 baseColor,
    vec3 specularTint,
    vec3 sheenTint,
    float anisotropic,
    float roughness,
    float subsurface,
    float clearcoatGloss,
    float eta,
    float metallic,
    float clearcoat,
    float specularTransmission,
    float sheen,
    bool didRefract,
    vec3 n,
    vec3 wi,
    vec3 wo,
    vec3 h,
    out float pdf
) {
    float diffuseWt = (1 - specularTransmission) * (1 - metallic);
    float metalWt = metallic;
    float clearcoatWt = 0.25 * clearcoat;
    float glassWt = (1 - metallic) * specularTransmission;

    float wtSum = diffuseWt + metalWt + glassWt + clearcoatWt;

    // vec3 diffuse(float roughness, float subsurface, vec3 baseColor, vec3 n, vec3 wi, vec3 wo, vec3 h)
    vec3 fdiffuse = evalDiffuse(roughness, subsurface, baseColor, n, wi, wo, h);
    float diffusePdf = pdfDiffuse(wo, n);

    // vec3 sheen(vec3 baseColor, vec3 wo, vec3 h, vec3 n, vec3 sheenTint)
    vec3 fsheen = evalSheen(baseColor, wo, h, n, sheenTint);

    // vec3 metal(mat3 tbn, vec3 baseColor, float anisotropic, float roughness, vec3 n, vec3 wi, vec3 wo, vec3 h, float specular, vec3 specularTint, float metallic, float eta) {
    vec3 fmetal = evalMetal(tbn, baseColor, anisotropic, roughness, n, wi, wo, h, specularTransmission, specularTint, metallic, eta);
    float metalPdf = pdfMetal(tbn, wi, wo, anisotropic, roughness);

    vec3 fclearcoat = evalClearcoat(tbn, wi, wo, clearcoatGloss, h);
    float clearcoatPdf = pdfClearcoat(tbn, wi, wo, h, clearcoatGloss);

    // vec3 glass(mat3 tbn, vec3 baseColor, float anisotropic, float roughness, float eta, vec3 n, vec3 wi, vec3 wo, bool didRefract, out float pdf)
    float glassPdf;
    vec3 fglass = glass(tbn, baseColor, anisotropic, roughness, eta, n, wi, wo, didRefract, glassPdf);

    pdf = diffusePdf * diffuseWt / wtSum +
          metalPdf * metalWt / wtSum +
          clearcoatPdf * clearcoatWt / wtSum +
          glassPdf * glassWt / wtSum;

    return diffuseWt / wtSum * (fdiffuse + fsheen * sheen) +
           metalWt / wtSum * fmetal +
           clearcoatWt * fclearcoat +
           glassWt / wtSum * fglass;
}

#endif