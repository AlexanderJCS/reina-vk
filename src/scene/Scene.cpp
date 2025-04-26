#include "Scene.h"

#include <vulkan/vulkan.h>

#include "Instances.h"

reina::scene::Scene::Scene() {

}

uint32_t reina::scene::Scene::defineObject(const std::string& filepath) {
    return models.addModel(filepath);
}

uint32_t reina::scene::Scene::defineTexture(const std::string& filepath) {
    // TODO: make it so that validation layer warnings do not occur if there are no textures in the scene
    if (textureFilepathsToID.find(filepath) != textureFilepathsToID.end()) {
        return textureFilepathsToID[filepath];
    }

    textureFilepathsToID[filepath] = nextImageID;
    nextImageID++;

    return textureFilepathsToID[filepath];
}

void reina::scene::Scene::addInstance(uint32_t objectID, glm::mat4 transform, const Material& mat) {
    instanceProperties.emplace_back(
            models.getModelRange(static_cast<int>(objectID)).indexOffset,
            mat.albedo,
            mat.emission,
            models.getModelRange(static_cast<int>(objectID)).tbnsIndexOffset,
            models.getModelRange(static_cast<int>(objectID)).texIndexOffset,
            mat.fuzzOrRefIdx,
            mat.interpNormals,
            mat.absorption,
            mat.textureID,
            mat.normalMapID,
            mat.bumpMapID,
            mat.cullBackface ? 1u : 0u
            );

    instancesToCreate.emplace_back(instanceProperties.size() - 1, objectID, transform);
}

void reina::scene::Scene::build(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue) {
    /*
     * Steps:
     * 1. Create textures
     * 2. Build models buffers
     * 3. Build BLASes
     * 4. Create instances
     * 5. Build TLAS
     * 6. Create instance properties buffer
     */

    // Step 1
    for (const auto& pair : textureFilepathsToID) {
        const std::string& filepath = pair.first;
        textures.push_back(reina::graphics::Image(logicalDevice, physicalDevice, cmdPool, queue, filepath));
    }

    // Step 2
    models.buildBuffers(logicalDevice, physicalDevice, cmdPool, queue);

    // Step 3
    blases.resize(models.getNumModels());
    for (size_t i = 0; i < models.getNumModels(); i++) {
        blases[i] = reina::graphics::Blas{logicalDevice, physicalDevice, cmdPool, queue, models, models.getModelRange(static_cast<int>(i)), true};
    }

    // Step 4
    std::vector<Instance> instancesVec;
    for (const auto& instanceToCreate : instancesToCreate) {
        instancesVec.emplace_back(
                blases[instanceToCreate.objectID],
                instanceProperties[instanceToCreate.instancePropertiesID].emission,
                models.getModelRange(static_cast<int>(instanceToCreate.objectID)),  // TODO: make model IDs ints from the model side
                models.getModelData(static_cast<int>(instanceToCreate.objectID)),
                instanceToCreate.instancePropertiesID,
                0,  // TODO: change
                instanceProperties[instanceToCreate.instancePropertiesID].cullBackface,
                instanceToCreate.transform
                );
    }

    instances = Instances{logicalDevice, physicalDevice, instancesVec};

    // Step 5
    tlas = vktools::createTlas(logicalDevice, physicalDevice, cmdPool, queue, instancesVec);

    // Step 6
    instancePropertiesBuffer = reina::core::Buffer{
            logicalDevice, physicalDevice, instanceProperties,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            static_cast<VkMemoryAllocateFlagBits>(0),
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
    };
}

void reina::scene::Scene::destroy(VkDevice logicalDevice) {
    instances.destroy(logicalDevice);
    instancePropertiesBuffer.destroy(logicalDevice);
    models.destroy(logicalDevice);

    auto vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
            vkGetDeviceProcAddr(logicalDevice, "vkDestroyAccelerationStructureKHR"));

    vkDestroyAccelerationStructureKHR(logicalDevice, tlas.accelerationStructure, nullptr);
    tlas.buffer.destroy(logicalDevice);

    for (auto& blas : blases) {
        blas.destroy(logicalDevice);
    }

    for (auto& texture : textures) {
        texture.destroy(logicalDevice);
    }
}

float reina::scene::Scene::getEmissiveWeight() {
    return instances.getEmissiveInstancesWeight();
}

const vktools::AccStructureInfo& reina::scene::Scene::getTlas() const {
    return tlas;
}

const reina::scene::Models& reina::scene::Scene::getModels() const {
    return models;
}

const reina::scene::Instances& reina::scene::Scene::getInstances() const {
    return instances;
}

const reina::core::Buffer &reina::scene::Scene::getInstancePropertiesBuffer() const {
    return instancePropertiesBuffer;
}

const std::vector<reina::graphics::Image> &reina::scene::Scene::getTextures() const {
    return textures;
}
