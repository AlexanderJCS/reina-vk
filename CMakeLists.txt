cmake_minimum_required(VERSION 3.29)
project(raygun_vk)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)
add_subdirectory(lib/glfw-3.4)

add_executable(raygun_vk main.cpp
        consts.h
        vktools.cpp
        vktools.h
        Window.cpp
        Window.h
        Shader.cpp
        Shader.h)

target_link_libraries(raygun_vk Vulkan::Vulkan glfw)

target_include_directories(raygun_vk PRIVATE ${Vulkan_INCLUDE_DIRS})
