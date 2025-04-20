#include <stdexcept>
#include "Instance.h"
#include "Models.h"

reina::scene::Instance::Instance(
        const reina::graphics::Blas& blas, glm::vec3 emission, reina::scene::ModelRange modelRange, const reina::scene::ObjData& objData,
        uint32_t objectPropertiesID, uint32_t materialOffset, bool cullBackface, glm::mat4x4 transform)
        : blas(blas), objectPropertiesID(objectPropertiesID), materialOffset(materialOffset), transform(transform), modelRange(modelRange), area(0), emission(emission), cullBackface(cullBackface) {

    if (isEmissive()) {
        computeCDF(objData, 0.2126 * emission.r + 0.7152 * emission.g + 0.0722 * emission.b);
    }
}

void reina::scene::Instance::computeCDF(const reina::scene::ObjData& objData, float brightness) {
    // transform all vertices into transform space
    std::vector<glm::vec3> transformedVertices = std::vector<glm::vec3>(objData.vertices.size() / 4);
    for (int i = 0; i < objData.vertices.size(); i += 4) {  // += 4 since each vertex is represented as a 4d vec
        float vx = objData.vertices[i + 0];
        float vy = objData.vertices[i + 1];
        float vz = objData.vertices[i + 2];

        transformedVertices[i / 4] = glm::vec3(glm::vec4(vx, vy, vz, 1) * transform);
    }

    // construct CDF with absolute values
    cdf = std::vector<float>(objData.indices.size() / 3);
    float cumulativeWeight = 0;
    for (int i = 0; i < objData.indices.size(); i += 3) {  // += 3 since 3 indices make a triangle
        uint32_t i0 = objData.indices[i + 0];
        uint32_t i1 = objData.indices[i + 1];
        uint32_t i2 = objData.indices[i + 2];

        glm::vec3 ab = transformedVertices[i1] - transformedVertices[i0];
        glm::vec3 ac = transformedVertices[i2] - transformedVertices[i0];

        float triArea = glm::length(glm::cross(ab, ac)) / 2;
        area += triArea;
        cumulativeWeight += triArea * brightness;
        cdf[i / 3] = cumulativeWeight;
    }

    if (cumulativeWeight == 0.0f) {
        throw std::runtime_error("Cannot calculate CDF for a mesh because the cumulative area is 0");
    }

    // normalize all values between [0, 1]
    for (float& value : cdf) {
        value /= cumulativeWeight;
    }

    weight = cumulativeWeight;
}

const reina::graphics::Blas& reina::scene::Instance::getBlas() const {
    return blas;
}

glm::mat4x4 reina::scene::Instance::getTransform() const {
    return transform;
}

uint32_t reina::scene::Instance::getObjectPropertiesID() const {
    return objectPropertiesID;
}

uint32_t reina::scene::Instance::getMaterialOffset() const {
    return materialOffset;
}

const std::vector<float> &reina::scene::Instance::getCDF() const {
    return cdf;
}

bool reina::scene::Instance::isEmissive() const {
    return glm::dot(emission, emission) > 0.00001f * 0.00001f;
}

reina::scene::ModelRange reina::scene::Instance::getModelRange() const {
    return modelRange;
}

float reina::scene::Instance::getArea() const {
    return area;
}

glm::vec3 reina::scene::Instance::getEmission() const {
    return emission;
}

float reina::scene::Instance::getWeight() const {
    return weight;
}

bool reina::scene::Instance::isCullBackface() const {
    return cullBackface;
}
