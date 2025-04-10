#ifndef RAYGUN_VK_INSTANCE_H
#define RAYGUN_VK_INSTANCE_H

#include "Blas.h"
#include "Models.h"

#include <glm/mat4x4.hpp>

namespace reina::graphics {
    class Instance {
    public:
        Instance(const Blas& blas, glm::vec3 emission, ModelRange modelRange, const ObjData& objData, uint32_t objectPropertiesID, uint32_t materialOffset, glm::mat4x4 transform = glm::mat4x4(1.0f));

        [[nodiscard]] const Blas& getBlas() const;
        [[nodiscard]] glm::mat4x4 getTransform() const;
        [[nodiscard]] uint32_t getObjectPropertiesID() const;
        [[nodiscard]] uint32_t getMaterialOffset() const;
        [[nodiscard]] float getArea() const;
        [[nodiscard]] const std::vector<float>& getCDF() const;
        [[nodiscard]] bool isEmissive() const;
        [[nodiscard]] glm::vec3 getEmission() const;
        [[nodiscard]] ModelRange getModelRange() const;

    private:
        void computeCDF(const ObjData& objData, float brightness);

        ModelRange modelRange;
        const Blas& blas;
        uint32_t objectPropertiesID = 0;
        uint32_t materialOffset = 0;
        glm::mat4x4 transform = glm::mat4x4(1.0f);
        std::vector<float> cdf;
        float area;
        glm::vec3 emission;
    };
}


#endif //RAYGUN_VK_INSTANCE_H
