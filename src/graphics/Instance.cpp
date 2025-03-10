#include <stdexcept>
#include "Instance.h"

reina::graphics::Instance::Instance(
        const reina::graphics::Blas& blas, bool emissive, const reina::graphics::ObjData& objData,
        uint32_t objectPropertiesID, uint32_t materialOffset, glm::mat4x4 transform)
        : blas(blas), objectPropertiesID(objectPropertiesID), materialOffset(materialOffset), transform(transform), emissive(emissive) {

    if (emissive) {
        computeCDF(objData);
    }
}

void reina::graphics::Instance::computeCDF(const reina::graphics::ObjData& objData) {
    // transform all vertices into transform space
    std::vector<glm::vec3> transformedVertices = std::vector<glm::vec3>(objData.vertices.size() / 4);
    for (int i = 0; i < transformedVertices.size(); i += 4) {  // += 4 since each vertex is represented as a 4d vec
        float vx = objData.vertices[i + 0];
        float vy = objData.vertices[i + 1];
        float vz = objData.vertices[i + 2];

        transformedVertices[i / 4] = glm::vec3(glm::vec4(vx, vy, vz, 0) * transform);
    }

    // construct CDF with absolute values
    cdf = std::vector<float>(objData.indices.size() / 3);
    float cumulativeArea = 0;
    for (int i = 0; i < objData.indices.size(); i += 3) {  // += 3 since 3 indices make a triangle
        uint32_t i0 = objData.indices[i + 0];
        uint32_t i1 = objData.indices[i + 1];
        uint32_t i2 = objData.indices[i + 2];

        glm::vec3 ab = transformedVertices[i1] - transformedVertices[i0];
        glm::vec3 ac = transformedVertices[i2] - transformedVertices[i0];

        cumulativeArea += glm::length(glm::cross(ab, ac)) / 2;
        cdf[i / 3] = cumulativeArea;
    }

    if (cumulativeArea == 0.0f) {
        throw std::runtime_error("Cannot calculate CDF for a mesh because the cumulative area is 0");
    }

    // normalize all values between [0, 1]
    for (float& value : cdf) {
        value /= cumulativeArea;
    }
}

const reina::graphics::Blas& reina::graphics::Instance::getBlas() const {
    return blas;
}

glm::mat4x4 reina::graphics::Instance::getTransform() const {
    return transform;
}

uint32_t reina::graphics::Instance::getObjectPropertiesID() const {
    return objectPropertiesID;
}

uint32_t reina::graphics::Instance::getMaterialOffset() const {
    return materialOffset;
}

const std::vector<float> &reina::graphics::Instance::getCDF() const {
    return cdf;
}

bool reina::graphics::Instance::isEmissive() const {
    return emissive;
}
