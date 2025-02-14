#include "Models.h"

#include <tiny_obj_loader.h>
#include <stdexcept>
#include <cmath>

rt::graphics::Models::Models(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, const std::vector<std::string>& modelFilepaths) {
    ObjData obj = getObjData(modelFilepaths[0]);

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    verticesBufferSize = obj.vertices.size();
    verticesBuffer = rt::core::Buffer{
            logicalDevice,
            physicalDevice,
            obj.vertices,
            usage,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    indicesBufferSize = obj.indices.size();
    indicesBuffer = rt::core::Buffer{
            logicalDevice,
            physicalDevice,
            obj.indices,
            usage,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };
}

rt::graphics::ObjData rt::graphics::Models::getObjData(const std::string& filepath) {
    tinyobj::ObjReader reader;
    reader.ParseFromFile(filepath);

    if (!reader.Valid()) {
        throw std::runtime_error("Error reading OBJ:\n" + reader.Error());
    }

    std::vector<tinyobj::real_t> tinyobjVertices = reader.GetAttrib().GetVertices();
    // convert from 3 floats per vertex to 4 floats per vertex. std::round removes any potential floating-point inaccuracies
    std::vector<float> objVertices(static_cast<int>(std::round(static_cast<double>(tinyobjVertices.size()) * 4.0/3)));

    for (int vertex = 0; vertex < tinyobjVertices.size() / 3; vertex++) {
        objVertices[vertex * 4] = tinyobjVertices[vertex * 3];
        objVertices[vertex * 4 + 1] = tinyobjVertices[vertex * 3 + 1];
        objVertices[vertex * 4 + 2] = tinyobjVertices[vertex * 3 + 2];
        objVertices[vertex * 4 + 3] = 0;
    }

    const std::vector<tinyobj::shape_t>& shapes = reader.GetShapes();
    if (shapes.size() != 1) {
        throw std::runtime_error("Several shapes to parse; need only one");
    }

    std::vector<uint32_t> objIndices(shapes[0].mesh.indices.size());
    for (int i = 0; i < shapes[0].mesh.indices.size(); i++) {
        objIndices[i] = shapes[0].mesh.indices[i].vertex_index;
    }

    return {objVertices, objIndices};
}

size_t rt::graphics::Models::getVerticesBufferSize() const {
    return verticesBufferSize;
}

size_t rt::graphics::Models::getIndicesBufferSize() const {
    return indicesBufferSize;
}

const rt::core::Buffer& rt::graphics::Models::getVerticesBuffer() const {
    return verticesBuffer.value();
}

const rt::core::Buffer &rt::graphics::Models::getIndicesBuffer() const {
    return indicesBuffer.value();
}

rt::graphics::ModelRange rt::graphics::Models::getModelRange(int index) const {
    return rt::graphics::ModelRange{0, 0, static_cast<uint32_t>(getIndicesBufferSize()) / 3};
}

void rt::graphics::Models::destroy(VkDevice logicalDevice) {
    if (verticesBuffer.has_value()) {
        verticesBuffer.value().destroy(logicalDevice);
    } if (indicesBuffer.has_value()) {
        indicesBuffer.value().destroy(logicalDevice);
    }
}
