#include "Shader.h"

#include <stdexcept>
#include <fstream>
#include <iostream>

Shader::Shader(VkDevice logicalDevice, const std::string& path, VkShaderStageFlagBits shaderStage) {
    this->shaderStage = shaderStage;
    this->logicalDevice = logicalDevice;
    shaderModule = createShaderModule(logicalDevice, readFile(path));
}

VkPipelineShaderStageCreateInfo Shader::pipelineShaderStageCreateInfo(const std::string& entryPoint) const {
    return VkPipelineShaderStageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = shaderStage,
        .module = shaderModule,
        .pName = entryPoint.c_str()
    };
}

std::vector<char> Shader::readFile(const std::string &filepath) {
    // std::ios::ate - start at the end of the file
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file at path: " + filepath);
    }

    // use the current pos to tell the size and allocate a buffer
    auto fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    // go to the beginning and read allat
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));

    file.close();

    return buffer;
}

VkShaderModule Shader::createShaderModule(VkDevice logicalDevice, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint32_t*>(code.data())
    };

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }

    return shaderModule;
}

void Shader::destroy() {
    if (shaderModule == VK_NULL_HANDLE) {
        return;
    }

    vkDestroyShaderModule(logicalDevice, shaderModule, nullptr);
    shaderModule = VK_NULL_HANDLE;
    logicalDevice = VK_NULL_HANDLE;  // probably not needed but good practice
}
