#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "Model.h"

rt::graphics::Model::Model(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, const std::string& objFilepath) {
    tinyobj::ObjReader reader;
    reader.ParseFromFile(objFilepath);

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

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    verticesBufferSize = objVertices.size();
    verticesBuffer = rt::core::Buffer{
        logicalDevice,
        physicalDevice,
        objVertices,
        usage,
        VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    indicesBufferSize = objIndices.size();
    indicesBuffer = rt::core::Buffer{
            logicalDevice,
            physicalDevice,
            objIndices,
            usage,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };
}

VkBuffer rt::graphics::Model::getVerticesBuffer() const {
    assert(verticesBuffer.has_value());
    return verticesBuffer.value().getBuffer();
}

VkBuffer rt::graphics::Model::getIndicesBuffer() const {
    assert(indicesBuffer.has_value());
    return indicesBuffer.value().getBuffer();
}

size_t rt::graphics::Model::getVerticesBufferSize() const {
    return verticesBufferSize;
}

size_t rt::graphics::Model::getIndicesBufferSize() const {
    return indicesBufferSize;
}

void rt::graphics::Model::destroy(VkDevice logicalDevice) {
    if (verticesBuffer.has_value()) {
        verticesBuffer.value().destroy(logicalDevice);
    } if (indicesBuffer.has_value()) {
        indicesBuffer.value().destroy(logicalDevice);
    }
}
