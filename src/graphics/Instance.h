#ifndef RAYGUN_VK_INSTANCE_H
#define RAYGUN_VK_INSTANCE_H

#include "Blas.h"

#include <glm/mat4x4.hpp>

namespace reina::graphics {
    struct Instance {
        const Blas& blas;
        uint32_t objectPropertiesID = 0;
        uint32_t materialOffset = 0;
        glm::mat4x4 transform = glm::mat4x4(1.0f);
    };
}


#endif //RAYGUN_VK_INSTANCE_H
