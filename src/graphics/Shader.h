#ifndef RAYGUN_VK_SHADER_H
#define RAYGUN_VK_SHADER_H

#include <vulkan/vulkan.h>
#include <string>
#include <vector>


namespace rt::graphics {
    class Shader {
    private:
        VkShaderModule shaderModule = VK_NULL_HANDLE;
        VkShaderStageFlagBits shaderStage;

        static std::vector<char> readFile(const std::string& filepath);
        static VkShaderModule createShaderModule(VkDevice logicalDevice, const std::vector<char>& code);

    public:
        Shader(VkDevice logicalDevice, const std::string& path, VkShaderStageFlagBits shaderStage);

        void destroy(VkDevice logicalDevice);

        [[nodiscard]] VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo(const std::string& entryPoint = "main") const;
    };
}


#endif //RAYGUN_VK_SHADER_H
