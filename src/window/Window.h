#ifndef RAYGUN_VK_WINDOW_H
#define RAYGUN_VK_WINDOW_H

#include "GLFW/glfw3.h"

namespace rt::window {
    class Window {
    private:
        GLFWwindow* glfwWindow;
        int width{}, height{};

    public:
        Window(int width, int height);

        [[nodiscard]] int getWidth() const;
        [[nodiscard]] int getHeight() const;
        [[nodiscard]] GLFWwindow* getGlfwWindow() const;

        bool isMinimized() const;

        bool shouldClose();

        void destroy();
    };
}

#endif //RAYGUN_VK_WINDOW_H
