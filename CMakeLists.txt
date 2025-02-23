cmake_minimum_required(VERSION 3.29)
project(reina_vk)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)
add_subdirectory(lib/glfw-3.4)

add_executable(reina_vk src/main.cpp
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
        src/core/Buffer.cpp
        src/core/Buffer.h
        src/graphics/ObjectProperties.h
        src/graphics/Blas.cpp
        src/graphics/Blas.h
        src/graphics/Instance.cpp
        src/graphics/Instance.h
        src/graphics/Models.cpp
        src/graphics/Models.h
        src/tools/Clock.cpp
        src/tools/Clock.h
        src/graphics/Camera.cpp
        src/graphics/Camera.h)

target_link_libraries(reina_vk Vulkan::Vulkan glfw)

target_include_directories(reina_vk PRIVATE ${Vulkan_INCLUDE_DIRS})
target_include_directories(reina_vk PUBLIC ${CMAKE_SOURCE_DIR}/lib/tiny_obj_loader)
