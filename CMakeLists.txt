cmake_minimum_required(VERSION 3.29)
project(reina_vk)

set(CMAKE_CXX_STANDARD 20)

# Use vcpkg via toolchain file (pass -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake when running CMake)
find_package(Vulkan REQUIRED)
find_package(assimp CONFIG REQUIRED)

include(FetchContent)

# GLFW
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
)
FetchContent_MakeAvailable(glfw)

# stb
FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

# tinyobjloader
FetchContent_Declare(
        tinyobjloader
        GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
        GIT_TAG        v2.0.0rc13
)
FetchContent_MakeAvailable(tinyobjloader)

FetchContent_Declare(
        fastgltf
        GIT_REPOSITORY https://github.com/spnda/fastgltf.git
        GIT_TAG        v0.8.0
)
FetchContent_MakeAvailable(fastgltf)

# tomlplusplus
FetchContent_Declare(
        tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG        v3.4.0
)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wno-error=unknown-pragmas)
endif()
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
        polyglot/raytrace.h
        src/core/PushConstants.h
        src/core/Buffer.cpp
        src/core/Buffer.h
        src/graphics/Blas.cpp
        src/graphics/Blas.h
        src/scene/Instance.cpp
        src/scene/Instance.h
        src/scene/Models.cpp
        src/scene/Models.h
        src/tools/Clock.cpp
        src/tools/Clock.h
        src/graphics/Camera.cpp
        src/graphics/Camera.h
        src/core/CmdBuffer.cpp
        src/core/CmdBuffer.h
        src/graphics/Image.cpp
        src/graphics/Image.h
        src/scene/Instances.cpp
        src/scene/Instances.h
        src/Reina.cpp
        src/Reina.h
        src/tools/SaveManager.cpp
        src/tools/SaveManager.h
        src/scene/Scene.cpp
        src/scene/Scene.h
        polyglot/bloom.h
        polyglot/tonemapping.h
        src/scene/gltf/gltfloader.h
        src/scene/gltf/gltfloader.cpp)

# Link libraries using keyword signature
target_link_libraries(reina_vk
        PRIVATE
        Vulkan::Vulkan
        glfw
        assimp::assimp
        fastgltf
)

# Include directories for FetchContent libraries
target_include_directories(reina_vk PRIVATE
        ${Vulkan_INCLUDE_DIRS}
        ${stb_SOURCE_DIR}
        ${tinyobjloader_SOURCE_DIR}
        ${tomlplusplus_SOURCE_DIR}
)
