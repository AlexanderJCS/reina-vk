#ifndef REINA_VK_SCENE_H
#define REINA_VK_SCENE_H

#include <stdint.h>
#include <string>
#include <glm/glm.hpp>

#include "../graphics/Image.h"
#include "../../polyglot/raytrace.h"

namespace reina::scene {
    struct Material {
        uint32_t textureID;
        uint32_t normalMapID;
        uint32_t bumpMapID;

        glm::vec3 albedo;
        glm::vec3 emission;
        float fuzzOrRefIdx;
        bool interpNormals;
        float absorption;
        bool cullBackface;
    };

    struct Object{
        ObjectProperties props;
        uint32_t modelID;
    };

    class Scene {
    public:
        Scene();

        /**
         * Define an object to be referenced by instances
         * @param filepath The filepath to the OBJ file
         * @returns The object ID
         */
        uint32_t defineObject(std::string filepath);

        /**
         * Define an image to be referenced by materials
         * @param image The image
         * @return The image ID
         */
        uint32_t defineImage(const reina::graphics::Image& image);

        /**
         * Add an instance to the scene. References an object with ObjectID
         * @param objectID The object ID
         * @param transform The object's transform
         * @param mat The material
         */
        void addInstance(uint32_t objectID, glm::mat4 transform, const Material& mat);

    private:

    };
}


#endif //REINA_VK_SCENE_H
