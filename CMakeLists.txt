cmake_minimum_required(VERSION 3.29)
project(raygun_vk)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)
add_subdirectory(lib/glfw-3.4)

add_executable(raygun_vk src/main.cpp
        src/tools/consts.h
        src/tools/vktools.cpp
        src/tools/vktools.h
        src/window/Window.cpp
        src/window/Window.h
        src/graphics/Shader.cpp
        src/graphics/Shader.h
        src/core/DescriptorSet.cpp
        src/core/DescriptorSet.h
        polyglot/common.h
        src/core/PushConstants.cpp
        src/core/PushConstants.h
        src/graphics/Model.cpp
        src/graphics/Model.h
        src/core/Buffer.cpp
        src/core/Buffer.h
        src/graphics/ObjectProperties.h)

target_link_libraries(raygun_vk Vulkan::Vulkan glfw)

target_include_directories(raygun_vk PRIVATE ${Vulkan_INCLUDE_DIRS})
target_include_directories(raygun_vk PUBLIC ${CMAKE_SOURCE_DIR}/lib/tiny_obj_loader)
