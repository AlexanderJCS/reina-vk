#include "shaderCommon.h.glsl"


float balanceHeuristic(float pdf1, float pdf2) {
    // use the square balance heuristic
    return pdf1 * pdf1 / (pdf1 * pdf1 + pdf2 * pdf2);
}


float pdfLambertian(vec3 normal, vec3 w0) {
    return dot(normal, w0) / k_pi;
}