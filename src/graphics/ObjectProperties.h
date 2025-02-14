#ifndef RAYGUN_VK_OBJECTPROPERTIES_H
#define RAYGUN_VK_OBJECTPROPERTIES_H

#include <glm/vec3.hpp>

namespace rt::graphics {
    struct alignas(16) ObjectProperties {
        glm::vec3 albedo;
        float padding = 0;
        glm::vec4 emission;  // xyz: emission RGB, w: emission strength
    };
}

#endif  // RAYGUN_VK_OBJECTPROPERTIES_H
