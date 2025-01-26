#include "Window.h"

#include <stdexcept>
#include <iostream>

window::Window::Window(int width, int height) {
    if (glfwInit() != GLFW_TRUE) {  // todo: should glfw init for every object or just once?
        throw std::runtime_error("Cannot init GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    glfwWindow = glfwCreateWindow(width, height, "Raygun", nullptr, nullptr);

    glfwGetWindowSize(glfwWindow, &this->width, &this->height);
}

int window::Window::getWidth() const {
    return width;
}

int window::Window::getHeight() const {
    return height;
}

void window::Window::destroy() {
    glfwDestroyWindow(glfwWindow);
    glfwTerminate();  // todo: verify if this is best practice.
}

GLFWwindow *window::Window::getGlfwWindow() const {
    return glfwWindow;
}

bool window::Window::shouldClose() {
    return glfwWindowShouldClose(glfwWindow);
}

bool window::Window::isMinimized() const {
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(glfwWindow, &fbWidth, &fbHeight);
    return fbWidth == 0 || fbHeight == 0;
}
