#ifndef RAYGUN_VK_OBJECTPROPERTIES_H
#define RAYGUN_VK_OBJECTPROPERTIES_H

#include <glm/vec3.hpp>

namespace reina::graphics {
    struct alignas(16) ObjectProperties {
        uint32_t indicesBytesOffset;
        glm::vec3 albedo;
        glm::vec4 emission;  // xyz: emission RGB, w: emission strength
        uint32_t normalsIndicesBytesOffset;  // same concept as indicesBytesOffset; it is here in the struct to prevent vec4 being aligned as 8 bytes, which the gpu doesn't like
        float fuzzOrRefIdx;  // fuzz of the material if metal, refractive index if dielectric. ignored for lambertian
        bool interpNormals;  // interpolate normals for smooth shading
        float absorption;  // the molar absorptivity (liters/mol/cm) times the molar concentration (mol/liter). used for rendering transparent dielectrics
    };
}

#endif  // RAYGUN_VK_OBJECTPROPERTIES_H
