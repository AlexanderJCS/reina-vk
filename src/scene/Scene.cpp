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

    instancesToCreate.emplace_back(instanceProperties.size() - 1, objectID, transform);
}

void reina::scene::Scene::build(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue) {
    /*
     * Steps:
     * 1. Create images
     * 2. Build models buffers
     * 3. Build BLASes
     * 4. Create instances
     * 5. Build TLAS
     */

    // Step 1
    for (const auto& pair : imageFilepathsToID) {
        const std::string& filepath = pair.first;
        images.push_back(reina::graphics::Image(logicalDevice, physicalDevice, cmdPool, queue, filepath));
    }

    // Step 2
    models.buildBuffers(logicalDevice, physicalDevice, cmdPool, queue);

    // Step 3
    blases.resize(models.getNumModels());
    for (size_t i = 0; i < models.getNumModels(); i++) {
        blases[i] = reina::graphics::Blas{logicalDevice, physicalDevice, cmdPool, queue, models, models.getModelRange(i), true};
    }

    // Step 4
    std::vector<Instance> instancesVec;
    for (const auto& instanceToCreate : instancesToCreate) {
        instancesVec.emplace_back(
                blases[instanceToCreate.objectID],
                instanceProperties[instanceToCreate.instancePropertiesID].emission,
                models.getModelRange(instanceToCreate.objectID),
                models.getModelData(instanceToCreate.objectID),
                instanceToCreate.instancePropertiesID,
                0,  // TODO: change
                instanceProperties[instanceToCreate.instancePropertiesID].cullBackface,
                instanceToCreate.transform
                );
    }

    // Step 5
    tlas = vktools::createTlas(logicalDevice, physicalDevice, cmdPool, queue, instances);
}

void reina::scene::Scene::destroy(VkDevice logicalDevice) {
    instances.destroy(logicalDevice);
    tlas.destroy(logicalDevice);

    for (auto& blas : blases) {
        blas.destroy(logicalDevice);
    }

    for (auto& image : images) {
        image.destroy(logicalDevice);
    }
}
