#include "Scene.h"

#include <vulkan/vulkan.h>

reina::scene::Scene::Scene() {

}

uint32_t reina::scene::Scene::defineObject(const std::string& filepath) {
    return models.addModel(filepath);
}

uint32_t reina::scene::Scene::defineImage(const std::string& filepath) {
    if (imageFilepathsToID.find(filepath) != imageFilepathsToID.end()) {
        return imageFilepathsToID[filepath];
    }

    imageFilepathsToID[filepath] = nextImageID;
    nextImageID++;

    return imageFilepathsToID[filepath];
}

void reina::scene::Scene::addInstance(uint32_t objectID, glm::mat4 transform, const Material& mat) {
    instanceProperties.emplace_back(
            models.getModelRange(objectID).indexOffset,
            mat.albedo,
            mat.emission,
            models.getModelRange(objectID).tbnsIndexOffset,
            models.getModelRange(objectID).texIndexOffset,
            mat.fuzzOrRefIdx,
            mat.interpNormals,
            mat.absorption,
            mat.normalMapID,
            mat.bumpMapID,
            mat.cullBackface ? 1u : 0u
            );

    instanceVec.emplace_back(instanceProperties.size() - 1, objectID, transform);
}

void reina::scene::Scene::destroy(VkDevice logicalDevice) {
    instances.destroy(logicalDevice);

    for (auto& image : images) {
        image.destroy(logicalDevice);
    }
}
