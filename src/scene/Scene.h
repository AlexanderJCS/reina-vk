#ifndef REINA_VK_SCENE_H
#define REINA_VK_SCENE_H

#include <stdint.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <glm/glm.hpp>

#include "../graphics/Image.h"
#include "Instance.h"
#include "../graphics/Blas.h"
#include "../tools/vktools.h"
#include "Instances.h"
#include "../../polyglot/raytrace.h"

#include <vulkan/vulkan.h>

namespace reina::scene {
    struct Material {
        uint32_t materialIdx;  // 0 = lambertian, 1 = metal, 2 = transparent dielectric

        int textureID;
        int normalMapID;
        int bumpMapID;

        glm::vec3 albedo;
        glm::vec3 emission;
        float roughness;
        float ior;
        bool interpNormals;
        float absorption;
        bool cullBackface;

        float anisotropic;
        float subsurface;
        float clearcoatGloss;
        glm::vec3 sheenTint;
        glm::vec3 specularTint;

        float metallic;
        float clearcoat;
        float specularTransmission;
        float sheen;
    };

    namespace {
        struct InstanceToCreate {
            uint32_t instancePropertiesID;
            uint32_t materialIdx;
            uint32_t objectID;
            glm::mat4 transform;
        };

        struct RawImageData {
            std::byte* imageData;
            size_t imageLengthBytes;
        };
    }

    class Scene {
    public:
        Scene() = default;

        /**
         * Define an object to be referenced by instances
         * @param filepath The filepath to the OBJ file
         * @returns The object ID
         */
        uint32_t defineObject(const std::string& filepath);

        /**
         * Define an object to be referenced by instances
         * @param modelData The model data of the object
         * @return The object ID
         */
        uint32_t defineObject(const ModelData& modelData);

        /**
         * Define a texture to be referenced by materials
         * @param image The image
         * @return The image ID
         */
        uint32_t defineTexture(const std::string& filepath);

        /**
         * Define a texture to be referenced by materials
         * @param image The image data
         * @return The image ID
         */
         uint32_t defineTexture(std::byte* imageData, size_t imageLengthBytes);

        /**
         * A combination of defineObject and addInstance. Intended for when only one instance is required; saves on
         * verbosity of defining an object then adding an instance.
         * @return The Object ID
         */
        uint32_t addObject(const std::string& filepath, glm::mat4 transform, const Material& mat);

        /**
         * A combination of defineObject and addInstance. Intended for when only one instance is required; saves on
         * verbosity of defining an object then adding an instance.
         * @return The Object ID
         */
        uint32_t addObject(const ModelData& modelData, glm::mat4 transform, const Material& mat);

        /**
         * Add an instance to the scene. References an object with ObjectID
         * @param objectID The object ID
         * @param transform The object's transform
         * @param mat The material
         */
        void addInstance(uint32_t objectID, glm::mat4 transform, const Material& mat);

        void build(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue);

        [[nodiscard]] float getEmissiveWeight();

        [[nodiscard]] const vktools::AccStructureInfo& getTlas() const;
        [[nodiscard]] const Models& getModels() const;
        [[nodiscard]] const Instances& getInstances() const;
        [[nodiscard]] const core::Buffer& getInstancePropertiesBuffer() const;
        [[nodiscard]] const std::vector<reina::graphics::Image>& getTextures() const;

        void destroy(VkDevice logicalDevice);

    private:
        Models models;
        std::vector<std::variant<std::string, RawImageData>> texturesToCreate;
        std::vector<InstanceToCreate> instancesToCreate;
        std::vector<InstanceProperties> instanceProperties;
        std::vector<reina::graphics::Blas> blases;
        Instances instances;
        std::vector<reina::graphics::Image> textures;
        vktools::AccStructureInfo tlas;
        reina::core::Buffer instancePropertiesBuffer;
    };
}


#endif //REINA_VK_SCENE_H
