cmake_minimum_required(VERSION 3.29)
project(reina_vk)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)

include(FetchContent)

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
)

FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

FetchContent_Declare(
        tinyobjloader
        GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
        GIT_TAG        v2.0.0rc13
)
FetchContent_MakeAvailable(tinyobjloader)

FetchContent_Declare(
        tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

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
        src/graphics/Camera.h
        src/core/CmdBuffer.cpp
        src/core/CmdBuffer.h
        src/graphics/Image.cpp
        src/graphics/Image.h
        src/graphics/Image.cpp
        src/graphics/Instances.cpp
        src/graphics/Instances.h
        src/Reina.cpp
        src/Reina.h
        src/tools/SaveManager.cpp
        src/tools/SaveManager.h)

target_link_libraries(reina_vk Vulkan::Vulkan glfw)

target_include_directories(reina_vk PRIVATE ${Vulkan_INCLUDE_DIRS})
target_include_directories(reina_vk PRIVATE ${stb_SOURCE_DIR})
target_include_directories(reina_vk PRIVATE ${tinyobjloader_SOURCE_DIR})
target_include_directories(reina_vk PRIVATE ${tomlplusplus_SOURCE_DIR})
