#include "Scene.h"

#include <vulkan/vulkan.h>

#include "Instances.h"

uint32_t reina::scene::Scene::defineObject(const std::string& filepath) {
    return models.addModel(filepath);
}

uint32_t reina::scene::Scene::defineObject(const reina::scene::ModelData &modelData) {
    return models.addModel(modelData);
}

uint32_t reina::scene::Scene::defineTexture(const std::string& filepath) {
    // TODO: make it so that validation layer warnings do not occur if there are no textures in the scene
    texturesToCreate.emplace_back(filepath);
    return static_cast<uint32_t>(texturesToCreate.size() - 1);
}


uint32_t reina::scene::Scene::defineTexture(std::byte* imageData, size_t imageLengthBytes) {
    texturesToCreate.emplace_back(RawImageData{imageData, imageLengthBytes});
    return static_cast<uint32_t>(texturesToCreate.size() - 1);
}

void reina::scene::Scene::addInstance(uint32_t objectID, glm::mat4 transform, const Material& mat) {
    instanceProperties.emplace_back(
            models.getModelRange(objectID).indexOffset,
            mat.albedo,
            mat.emission,
            models.getModelRange(objectID).tbnsIndexOffset,
            models.getModelRange(objectID).texIndexOffset,
            mat.fuzz,
            mat.ior,
            mat.interpNormals,
            mat.absorption,
            mat.textureID,
            mat.normalMapID,
            mat.bumpMapID,
            mat.cullBackface ? 1u : 0u
            );

    instancesToCreate.emplace_back(instanceProperties.size() - 1, mat.materialIdx, objectID, transform);
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
    for (const auto& texToCreate : texturesToCreate) {
        std::visit(
            [&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    textures.emplace_back(logicalDevice, physicalDevice, cmdPool, queue, arg);
                } else {
                    textures.emplace_back(
                            logicalDevice,
                            physicalDevice,
                            cmdPool,
                            queue,
                            arg.imageData,
                            arg.imageLengthBytes
                    );
                }
            },
            texToCreate
        );
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
                instanceToCreate.materialIdx,
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

uint32_t reina::scene::Scene::addObject(const std::string& filepath, glm::mat4 transform,
                                        const reina::scene::Material& mat) {
    uint32_t objectID = defineObject(filepath);
    addInstance(objectID, transform, mat);

    return objectID;
}

uint32_t reina::scene::Scene::addObject(const reina::scene::ModelData &modelData, glm::mat4 transform,
                                        const reina::scene::Material &mat) {
    uint32_t objectID = defineObject(modelData);
    addInstance(objectID, transform, mat);

    return objectID;
}
