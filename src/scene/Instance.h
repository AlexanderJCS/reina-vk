#ifndef RAYGUN_VK_INSTANCE_H
#define RAYGUN_VK_INSTANCE_H

#include "../graphics/Blas.h"
#include "Models.h"

#include <glm/mat4x4.hpp>

namespace reina::scene {
    class Instance {
    public:
        Instance(const reina::graphics::Blas& blas, glm::vec3 emission, reina::scene::ModelRange modelRange, const reina::scene::ModelData& modelData, uint32_t instancePropertiesID, uint32_t materialOffset, bool cullBackface, glm::mat4x4 transform = glm::mat4x4(1.0f));

        [[nodiscard]] const reina::graphics::Blas& getBlas() const;
        [[nodiscard]] glm::mat4x4 getTransform() const;
        [[nodiscard]] uint32_t getInstancePropertiesID() const;
        [[nodiscard]] uint32_t getMaterialOffset() const;
        [[nodiscard]] float getArea() const;
        [[nodiscard]] const std::vector<float>& getCDF() const;
        [[nodiscard]] bool isEmissive() const;
        [[nodiscard]] glm::vec3 getEmission() const;
        [[nodiscard]] reina::scene::ModelRange getModelRange() const;
        [[nodiscard]] float getWeight() const;
        [[nodiscard]] bool isCullBackface() const;

    private:
        void computeCDF(const reina::scene::ModelData& objData, float brightness);

        reina::scene::ModelRange modelRange;
        const reina::graphics::Blas& blas;
        uint32_t instancePropertiesID = 0;
        uint32_t materialOffset = 0;
        glm::mat4x4 transform = glm::mat4x4(1.0f);
        std::vector<float> cdf;
        float area = 0;
        float weight = 0;
        glm::vec3 emission;
        bool cullBackface;
    };
}


#endif //RAYGUN_VK_INSTANCE_H
