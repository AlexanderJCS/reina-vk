#ifndef REINA_VK_SCENE_H
#define REINA_VK_SCENE_H

#include <stdint.h>
#include <string>
#include <vector>
#include <unordered_map>
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
        int textureID;
        int normalMapID;
        int bumpMapID;

        glm::vec3 albedo;
        glm::vec3 emission;
        float fuzzOrRefIdx;
        bool interpNormals;
        float absorption;
        bool cullBackface;
    };

    namespace {
        struct InstanceToCreate {
            uint32_t instancePropertiesID;
            uint32_t objectID;
            glm::mat4 transform;
        };
    }

    class Scene {
    public:
        Scene();

        /**
         * Define an object to be referenced by instances
         * @param filepath The filepath to the OBJ file
         * @returns The object ID
         */
        uint32_t defineObject(const std::string& filepath);

        /**
         * Define an image to be referenced by materials
         * @param image The image
         * @return The image ID
         */
        uint32_t defineImage(const std::string& filepath);

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
        uint32_t nextImageID = 0;
        std::unordered_map<std::string, uint32_t> imageFilepathsToID;
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
