#ifndef RAYGUN_VK_OBJECTPROPERTIES_H
#define RAYGUN_VK_OBJECTPROPERTIES_H

#include <glm/vec3.hpp>

namespace rt::graphics {
    struct alignas(16) ObjectProperties {
        uint32_t indicesBytesOffset;
        glm::vec3 albedo;
        glm::vec4 emission;  // xyz: emission RGB, w: emission strength
        float fuzzOrRefIdx;  // fuzz of the material if metal, refractive index if dielectric. ignored for lambertian
        glm::vec3 padding2 = glm::vec3(0.0f);
    };
}

#endif  // RAYGUN_VK_OBJECTPROPERTIES_H
