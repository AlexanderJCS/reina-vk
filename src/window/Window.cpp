#include "Window.h"

#include <stdexcept>
#include <iostream>

reina::window::Window::Window(int width, int height) {
    if (glfwInit() != GLFW_TRUE) {  // todo: should glfw init for every object or just once?
        throw std::runtime_error("Cannot init GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    glfwWindow = glfwCreateWindow(width, height, "Raygun", nullptr, nullptr);

    glfwGetWindowSize(glfwWindow, &this->width, &this->height);
}

int reina::window::Window::getWidth() const {
    return width;
}

int reina::window::Window::getHeight() const {
    return height;
}

void reina::window::Window::destroy() {
    glfwDestroyWindow(glfwWindow);
    glfwTerminate();  // todo: verify if this is best practice.
}

GLFWwindow* reina::window::Window::getGlfwWindow() const {
    return glfwWindow;
}

bool reina::window::Window::shouldClose() const {
    return glfwWindowShouldClose(glfwWindow);
}

bool reina::window::Window::isMinimized() const {
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(glfwWindow, &fbWidth, &fbHeight);
    return fbWidth == 0 || fbHeight == 0;
}

bool reina::window::Window::keyPressed(int glfwKey) const {
    return glfwGetKey(glfwWindow, glfwKey) == GLFW_PRESS;
}
