#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "closestHitCommon.h.glsl"
#include "nee.h.glsl"
#include "texutils.h.glsl"

const float INV_PI = 0.3183098862;

// Resource: https://schuttejoe.github.io/post/disneybsdf/

// ==========================================================================
//                                   Math
// ==========================================================================
float cosTheta(const vec3 v) {
    // Assumes v is y-up in tangent space
    // https://github.com/schuttejoe/Selas/blob/56a7fab5a479ec93d7f641bb64b8170f3b0d3095/Source/Core/MathLib/Trigonometric.h#L78
    return v.y;
}

float absCosTheta(const vec3 w) {
    // Assumes v is y-up in tangent space
    // https://github.com/schuttejoe/Selas/blob/56a7fab5a479ec93d7f641bb64b8170f3b0d3095/Source/Core/MathLib/Trigonometric.h#L78
    return abs(cosTheta(w));
}

float cos2Theta(const vec3 w) {
    // Assumes v is y-up in tangent space
    // https://github.com/schuttejoe/Selas/blob/56a7fab5a479ec93d7f641bb64b8170f3b0d3095/Source/Core/MathLib/Trigonometric.h#L78
    return w.y * w.y;
}

float sin2Theta(const vec3 w)
{
    return max(0.0f, 1.0f - cos2Theta(w));
}

float sinTheta(const vec3 w) {
    return sqrt(sin2Theta(w));
}

float tanTheta(const vec3 w) {
    return sinTheta(w) / cosTheta(w);
}

float cosPhi(const vec3 w) {
    float sinTheta = sinTheta(w);
    return (sinTheta == 0) ? 1.0f : clamp(w.x / sinTheta, -1.0f, 1.0f);
}

float sinPhi(const vec3 w) {
    float sinTheta = sinTheta(w);
    return (sinTheta == 0) ? 1.0f : clamp(w.z / sinTheta, -1.0f, 1.0f);
}

float cos2Phi(const vec3 w) {
    float cosPhi = cosPhi(w);
    return cosPhi * cosPhi;
}

float sin2Phi(const vec3 w) {
    float sinPhi = sinPhi(w);
    return sinPhi * sinPhi;
}

// ==========================================================================
//                                 Fresnel
// ==========================================================================
float schlickWeight(float u) {
    // https://github.com/schuttejoe/Selas/blob/dev/Source/Core/Shading/Fresnel.cpp
    float m = Saturate(1.0f - u);
    float m2 = m * m;
    return m * m2 * m2;
}

vec3 schlick(vec3 r0, float radians) {
    // https://github.com/schuttejoe/Selas/blob/dev/Source/Core/Shading/Fresnel.cpp
    float exponential = pow(1.0f - radians, 5.0f);
    return r0 + (vec3(1.0f) - r0) * exponential;
}

float schlick(float r0, float radians) {
    // https://github.com/schuttejoe/Selas/blob/dev/Source/Core/Shading/Fresnel.cpp
    return mix(1.0f, schlickWeight(radians), r0);
}

// ===========================================================================
//                  TODO: Not sure what to label this yet
// ==========================================================================
struct AnisotropicParams {
    float ax;
    float ay;
};

void calculateAnisotropicParams(float roughness, float anisotropic) {
    float aspect = sqrt(1.0f - 0.9f * anisotropic);

    return AnisotropicParams(
        max(0.001f, pow(roughness, 2.0) / aspect),
        max(0.001f, pow(roughness, 2.0) * aspect)
    );
}

// =========================================================================
//                                 Sheen
// ==========================================================================
vec3 calculateTint(vec3 baseColor) {
    // https://schuttejoe.github.io/post/disneybsdf/
    // Done this way in BRDF explorer
    // I'm not sure why the blue channel is multiplied by 1.0
    float luminance = dot(baseColor, vec3(0.3, 0.6, 1.0));
    return (luminance > 0.0) ? baseColor / luminance : vec3(1.0);
}

vec3 evaluateSheen(const InstanceProperties props, const vec3 wo, const vec3 wm, const vec3 wi) {
    if (props.sheen <= 0.0) {
        return vec3(0.0);
    }

    float dotHL = dot(wm, wi);
    vec3 tint = calculateTint(props.albedo);
    return props.sheen * mix(vec3(1.0), tint, props.sheenTint) * schlickWeight(dotHL);
}

// ==========================================================================
//                                Clearcoat
// ==========================================================================
float gtr1(float absDotHL, float a) {
    // https://schuttejoe.github.io/post/disneybsdf/
    if (a >= 1) {
        return INV_PI;
    }

    float a2 = a * a;
    return (a2 - 1.0f) / (K_PI * log2(a2) * (1.0f + (a2 - 1.0f) * absDotHL * absDotHL));
}

float separableSmithGGXG1(const vec3 w, float a) {
    // https://schuttejoe.github.io/post/disneybsdf/
    float a2 = a * a;
    float absDotNV = absCosTheta(w);

    return 2.0f / (1.0f + sqrt(a2 + (1 - a2) * absDotNV * absDotNV));
}

struct ClearcoatEvaluation{
    float fPdfW;
    float rPdfW;
    float clearcoat;
};

float evaluateDisneyClearcoat(float clearcoat, float alpha, const vec3 wo, const vec3 wm, const vec3 wi) {
    if(clearcoat <= 0.0f) {
        return 0.0f;
    }

    float absDotNH = absCosTheta(wm);
    float absDotNL = absCosTheta(wi);
    float absDotNV = absCosTheta(wo);
    float dotHL = Dot(wm, wi);

    float d = gtr1(absDotNH, mix(0.1f, 0.001f, alpha));
    float f = schlick(0.04f, dotHL);  // 0.04 is the fresnel term for ROI = 1.5, modelling polyurethane clearcoat
    float gl = separableSmithGGXG1(wi, 0.25f);
    float gv = separableSmithGGXG1(wo, 0.25f);

    float fPdfW = d / (4.0f * absDotNL);
    float rPdfW = d / (4.0f * absDotNV);

    return ClearcoatEvaluation(
            fPdfW,
            rPdfW,
            0.25f * clearcoat * d * f * gl * gv
    );
}

// ==========================================================================
//                                 Specular
// ==========================================================================
float ggxAnisotropicD(const vec3 wm, float ax, float ay) {
    float dotHX2 = pow(wm.x, 2.0);
    float dotHY2 = pow(wm.z, 2.0);
    float cos2Theta = cos2Theta(wm);
    float ax2 = pow(ax, 2.0);
    float ay2 = pow(ay, 2.0);

    return 1.0f / (K_PI * ax * ay * pow(dotHX2 / ax2 + dotHY2 / ay2 + cos2Theta, 2.0));
}

float separableSmithGGXG1(const vec3 w, const vec3 wm, float ax, float ay) {
    float dotHW = dot(w, wm);
    if (dotHW <= 0.0f) {
        return 0.0f;
    }

    float absTanTheta = abs(tanTheta(w));
    if (isinf(absTanTheta)) {
        return 0.0f;
    }

    float a = sqrt(cos2Phi(w) * ax * ax + sin2Phi(w) * ay * ay);
    float a2Tan2Theta = pow(a * absTanTheta, 2.0);

    float lambda = 0.5f * (-1.0f + sqrt(1.0f + a2Tan2Theta));
    return 1.0f / (1.0f + lambda);
}

float schlickR0FromRelativeIOR(float eta) {
    // https://github.com/schuttejoe/Selas/blob/56a7fab5a479ec93d7f641bb64b8170f3b0d3095/Source/Core/Shading/Fresnel.cpp#L79
    // https://seblagarde.wordpress.com/2013/04/29/memo-on-fresnel-equations/
    return pow(eta - 1.0f, 2.0) / pow(eta + 1.0f, 2.0);
}

float dielectric(float cosThetaI, float ni, float nt) {
    // Copied from PBRT. This function calculates the full Fresnel term for a dielectric material.
    // https://github.com/schuttejoe/Selas/blob/56a7fab5a479ec93d7f641bb64b8170f3b0d3095/Source/Core/Shading/Fresnel.cpp#L79

    cosThetaI = clamp(cosThetaI, -1.0f, 1.0f);

    // Swap index of refraction if this is coming from inside the surface
    if(cosThetaI < 0.0f) {
        float temp = ni;
        ni = nt;
        nt = temp;

        cosThetaI = -cosThetaI;
    }

    float sinThetaI = sqrt(max(0.0f, 1.0f - cosThetaI * cosThetaI));
    float sinThetaT = ni / nt * sinThetaI;

    // Total internal reflection
    if (sinThetaT >= 1) {
        return 1;
    }

    float cosThetaT = sqrt(max(0.0f, 1.0f - sinThetaT * sinThetaT));

    float rParallel     = ((nt * cosThetaI) - (ni * cosThetaT)) / ((nt * cosThetaI) + (ni * cosThetaT));
    float rPerpendicuar = ((ni * cosThetaI) - (nt * cosThetaT)) / ((ni * cosThetaI) + (nt * cosThetaT));
    return (rParallel * rParallel + rPerpendicuar * rPerpendicuar) / 2;
}

vec3 disneyFresnel(const InstanceProperties props, const vec3 wo, const vec3 wm, const vec3 wi) {
    float dotHV = Absf(Dot(wm, wo));

    vec3 tint = CalculateTint(props.baseColor);

    // -- See section 3.1 and 3.2 of the 2015 PBR presentation + the Disney BRDF explorer (which does their
    // -- 2012 remapping rather than the SchlickR0FromRelativeIOR seen here but they mentioned the switch in 3.2).
    vec3 R0 = schlickR0FromRelativeIOR(props.fuzzOrRefIdx) * Lerp(float3(1.0f), tint, props.specularTint);
    R0 = Lerp(R0, props.baseColor, props.metallic);

    float dielectricFresnel = dielectric(dotHV, 1.0f, props.fuzzOrRefIdx);
    vec3 metallicFresnel = schlick(R0, dot(wi, wm));

    return mix(vec3(dielectricFresnel), metallicFresnel, props.metallic);
}

struct DisneyBRDF {
    float fPdf;
    float rPdf;
    vec3 f;
};

DisneyBRDF evaluateDisneyBRDF(const InstanceProperties props, const vec3 wo, const vec3 wm, const vec3 wi) {
    float fPdf = 0.0f;
    float rPdf = 0.0f;

    float dotNL = cosTheta(wi);
    float dotNV = cosTheta(wo);
    if(dotNL <= 0.0f || dotNV <= 0.0f) {
        return vec3(0);
    }

    AnisotropicParams params = calculateAnisotropicParams(props.roughness, props.anisotropic);

    float d = ggxAnisotropicD(wm, params.ax, params.ay);
    float gl = separableSmithGGXG1(wi, wm, params.ax, params.ay);
    float gv = separableSmithGGXG1(wo, wm, params.ax, params.ay);

    vec3 f = disneyFresnel(props, wo, wm, wi);

    ggxVndfAnisotropicPdf(wi, wm, wo, params.ax, params.ay, fPdf, rPdf);
    fPdf *= (1.0f / (4 * abs(dot(wo, wm))));
    rPdf *= (1.0f / (4 * abs(dot(wi, wm))));

    return DisneyBRDF(
        fPdf,
        rPdf,
        d * gl * gv * f / (4.0f * dotNL * dotNV)
    );
}

// =========================================================================
//                                 Diffuse
// =========================================================================

float evaluateDisneyRetroDiffuse(const InstanceProperties props, const vec3 wo, const vec3 wm, const vec3 wi)
{
    float dotNL = absCosTheta(wi);
    float dotNV = absCosTheta(wo);

    float roughness = props.roughness * props.roughness;

    float rr = 0.5f + 2.0f * dotNL * dotNL * roughness;
    float fl = schlickWeight(dotNL);
    float fv = schlickWeight(dotNV);

    return rr * (fl + fv + fl * fv * (rr - 1.0f));
}

float evaluateDisneyDiffuse(const InstanceProperties props, const vec3 wo, const vec3 wm, const vec3 wi, bool thin) {
    float dotNL = absCosTheta(wi);
    float dotNV = absCosTheta(wo);

    float fl = schlickWeight(dotNL);
    float fv = schlickWeight(dotNV);

    float hanrahanKrueger = 0.0f;

    if (thin && props.flatness > 0.0f) {
        float roughness = props.roughness * props.roughness;

        float dotHL = dot(wm, wi);
        float fss90 = dotHL * dotHL * roughness;
        float fss = mix(1.0f, fss90, fl) * mix(1.0f, fss90, fv);

        float ss = 1.25f * (fss * (1.0f / (dotNL + dotNV) - 0.5f) + 0.5f);
        hanrahanKrueger = ss;
    }

    float lambert = 1.0f;
    float retro = evaluateDisneyRetroDiffuse(props, wo, wm, wi);
    float subsurfaceApprox = Lerp(lambert, hanrahanKrueger, thin ? props.flatness : 0.0f);

    return INV_PI * (retro + subsurfaceApprox * (1.0f - 0.5f * fl) * (1.0f - 0.5f * fv));
}

// =========================================================================
//                                  Together
// ========================================================================
struct LobePDFsResult {
    float pSpecular;
    float pSpecTrans;
    float pDiffuse;
    float pClearcoat;
};

LobePDFsResult calculateLobePdfs(const InstanceProperties props) {
    float metallicBRDF   = props.metallic;
    float specularBSDF   = (1.0f - props.metallic) * props.specTrans;
    float dielectricBRDF = (1.0f - props.specTrans) * (1.0f - props.metallic);

    float specularWeight     = metallicBRDF + dielectricBRDF;
    float transmissionWeight = specularBSDF;
    float diffuseWeight      = dielectricBRDF;
    float clearcoatWeight    = clamp(props.clearcoat, 0.0, 1.0);

    float norm = 1.0f / (specularWeight + transmissionWeight + diffuseWeight + clearcoatWeight);

    return LobePDFsResult(
        norm * specularWeight,
        norm * transmissionWeight,
        norm * diffuseWeight,
        norm * clearcoatWeight
    );
}

struct DisneyEval {
    float forwardPdf;
    float reversePdf;
    vec3 reflectance;
};

DisneyEval evaluateDisney(const InstanceProperties props, vec3 v, vec3 l, bool thin) {
    vec3 wo = normalize(MatrixMultiply(v, surface.worldToTangent));
    vec3 wi = normalize(MatrixMultiply(l, surface.worldToTangent));
    vec3 wm = normalize(wo + wi);

    float dotNV = cosTheta(wo);
    float dotNL = cosTheta(wi);

    vec3 reflectance = vec3(0);
    float forwardPdf = 0.0f;
    float reversePdf = 0.0f;

    LobePDFsResult lobePDFs = calculateLobePdfs(props);

    vec3 baseColor = props.baseColor;
    float metallic = props.metallic;
    float specTrans = props.specTrans;
    float roughness = props.roughness;

    // calculate all of the anisotropic params
    AnisotropicParams params = calculateAnisotropicParams(props.roughness, props.anisotropic);

    float diffuseWeight = (1.0f - metallic) * (1.0f - specTrans);
    float transWeight   = (1.0f - metallic) * specTrans;

    // -- Clearcoat
    bool upperHemisphere = bool(dotNL > 0.0f && dotNV > 0.0f);
    if (upperHemisphere && props.clearcoat > 0.0f) {
        float forwardClearcoatPdfW;
        float reverseClearcoatPdfW;

        float clearcoat = evaluateDisneyClearcoat(props.clearcoat, props.clearcoatGloss, wo, wm, wi, forwardClearcoatPdfW, reverseClearcoatPdfW);
        reflectance += vec3(clearcoat);
        forwardPdf += lobePDFs.pClearcoat * forwardClearcoatPdfW;
        reversePdf += lobePDFs.pClearcoat * reverseClearcoatPdfW;
    }

    // -- Diffuse
    if (diffuseWeight > 0.0f) {
        float forwardDiffusePdfW = absCosTheta(wi);
        float reverseDiffusePdfW = absCosTheta(wo);
        float diffuse = evaluateDisneyDiffuse(props, wo, wm, wi, thin);

        vec3 sheen = evaluateSheen(props, wo, wm, wi);

        reflectance += diffuseWeight * (diffuse * props.baseColor + sheen);

        forwardPdf += lobePDFs.pDiffuse * forwardDiffusePdfW;
        reversePdf += lobePDFs.pDiffuse * reverseDiffusePdfW;
    }

    // -- Transmission
    if (transWeight > 0.0f) {
        // Scale roughness based on IOR (Burley 2015, Figure 15).
        float rscaled = thin ? thinTransmissionRoughness(surface.ior, props.roughness) : props.roughness;
        AnisotropicParams t = calculateAnisotropicParams(rscaled, props.anisotropic);

        vec3 transmission = evaluateDisneySpecTransmission(props, wo, wm, wi, t.ax, t.ay, thin);
        reflectance += transWeight * transmission;

        float forwardTransmissivePdfW;
        float reverseTransmissivePdfW;
        ggxVndfAnisotropicPdf(wi, wm, wo, t.ax, t.ay, forwardTransmissivePdfW, reverseTransmissivePdfW);

        float dotLH = dot(wm, wi);
        float dotVH = dot(wm, wo);
        forwardPdf += lobePDFs.pSpecTrans * forwardTransmissivePdfW / (pow(dotLH + props.fuzzOrRefIdx * dotVH, 2.0));
        reversePdf += lobePDFs.pSpecTrans * reverseTransmissivePdfW / (pow(dotVH + props.fuzzOrRefIdx * dotLH, 2.0));
    }

    // -- specular
    if (upperHemisphere) {
        DisneyBRDF brdf = evaluateDisneyBRDF(props, wo, wm, wi);

        reflectance += brdf.f;
        forwardPdf += lobePDFs.pSpecular * brdf.fPdf / (4 * abs(dot(wo, wm)));
        reversePdf += lobePDFs.pSpecular * brdf.rPdf / (4 * abs(dot(wi, wm)));
    }

    reflectance = reflectance * Absf(dotNL);

    return reflectance;
}

// =========================================================================
//                               Sampling
// =========================================================================

BsdfSample SampleDisneySpecTransmission(CSampler s, const SurfaceParameters& surface, float3 v, bool thin) {
    float3 wo = MatrixMultiply(v, surface.worldToTangent);
    if(CosTheta(wo) == 0.0) {
        sample.forwardPdfW = 0.0f;
        sample.reversePdfW = 0.0f;
        sample.reflectance = vec3(0);
        sample.wi = vec3(0);
        return false;
    }

    // -- Scale roughness based on IOR
    float rscaled = thin ? ThinTransmissionRoughness(surface.ior, surface.roughness) : surface.roughness;

    AnisotropicParams t = calculateAnisotropicParams(rscaled, surface.anisotropic);

    // -- Sample visible distribution of normals
    float r0 = s->UniformFloat();
    float r1 = s->UniformFloat();
    float3 wm = sampleGgxVndfAnisotropic(wo, tax, tay, r0, r1);

    float dotVH = Dot(wo, wm);
    if(wm.y < 0.0f) {
        dotVH = -dotVH;
    }

    float ni = wo.y > 0.0f ? 1.0f : surface.fuzzOrRefIdx;
    float nt = wo.y > 0.0f ? surface.fuzzOrRefIdx : 1.0f;
    float relativeIOR = ni / nt;

    // -- Disney uses the full dielectric Fresnel equation for transmission. We also importance sample F
    // -- to switch between refraction and reflection at glancing angles.
    float F = dielectric(dotVH, 1.0f, surface.fuzzOrRefIdx);

    // -- Since we're sampling the distribution of visible normals the pdf cancels out with a number of other terms.
    // -- We are left with the weight G2(wi, wo, wm) / G1(wi, wm) and since Disney uses a separable masking function
    // -- we get G1(wi, wm) * G1(wo, wm) / G1(wi, wm) = G1(wo, wm) as our weight.
    float G1v = SeparableSmithGGXG1(wo, wm, tax, tay);

    float pdf;

    float3 wi;
    if (sampler->UniformFloat() <= F) {
        wi = Normalize(Reflect(wm, wo));

        sample.flags = SurfaceEventFlags::eScatterEvent;
        sample.reflectance = G1v * surface.baseColor;

        float jacobian = (4 * abs(dot(wo, wm)));
        pdf = F / jacobian;
    }
    else {
        if (thin) {
            // -- When the surface is thin so it refracts into and then out of the surface during this shading event.
            // -- So the ray is just reflected then flipped and we use the sqrt of the surface color.
            wi = Reflect(wm, wo);
            wi.y = -wi.y;
            sample.reflectance = G1v * Sqrt(surface.baseColor);

            // -- Since this is a thin surface we are not ending up inside of a volume so we treat this as a scatter event.
            sample.flags = SurfaceEventFlags::eScatterEvent;
        }
        else {
            if (Transmit(wm, wo, relativeIOR, wi)) {
                sample.flags = SurfaceEventFlags::eTransmissionEvent;
                sample.medium.phaseFunction = dotVH > 0.0f ? MediumPhaseFunction::eIsotropic : MediumPhaseFunction::eVacuum;
                sample.medium.extinction = CalculateExtinction(surface.transmittanceColor, surface.scatterDistance);
            }
            else {
                sample.flags = SurfaceEventFlags::eScatterEvent;
                wi = reflect(wm, wo);
            }

            sample.reflectance = G1v * surface.baseColor;
        }

        wi = normalize(wi);

        float dotLH = Absf(Dot(wi, wm));
        float jacobian = dotLH / (Square(dotLH + surface.relativeIOR * dotVH));
        pdf = (1.0f - F) / jacobian;
    }

    if(cosTheta(wi) == 0.0f) {
        sample.forwardPdfW = 0.0f;
        sample.reversePdfW = 0.0f;
        sample.reflectance = float3::Zero_;
        sample.wi = float3::Zero_;
        return false;
    }

    if(surface.roughness < 0.01f) {
        // -- This is a hack to allow us to sample the correct IBL texture when a path bounced off a smooth surface.
        sample.flags |= SurfaceEventFlags::eDiracEvent;
    }

    // -- calculate VNDF pdf terms and apply Jacobian and Fresnel sampling adjustments
    ggxVndfAnisotropicPdf(wi, wm, wo, tax, tay, sample.forwardPdfW, sample.reversePdfW);
    sample.forwardPdfW *= pdf;
    sample.reversePdfW *= pdf;

    // -- convert wi back to world space
    sample.wi = normalize(MatrixMultiply(wi, MatrixTranspose(surface.worldToTangent)));

    return true;
}