

#ifndef REINA_VK_CAMERA_H
#define REINA_VK_CAMERA_H

#include <glm/glm.hpp>

#include "../window/Window.h"

namespace reina::graphics {
    class Camera {
    public:
        Camera(const reina::window::Window& renderWindow, float fov, float aspectRatio, glm::vec3 pos, glm::vec3 cameraFront);

        /**
         * Process input. Returns if the camera position changed. Important for "clearing" the screen and starting the
         * render fresh when the camera changed.
         * @param window The render preview window.
         * @param timeDelta The delta time between frames in seconds.
         * @return If the camera position changed
         */
        void processInput(const reina::window::Window& window, double timeDelta);

        static void mouseCallback(GLFWwindow* window, double xpos, double ypos);

        [[nodiscard]] const glm::mat4& getInverseView() const;
        [[nodiscard]] const glm::mat4& getInverseProjection() const;

        [[nodiscard]] bool hasChanged() const;
        void refresh();

    private:
        glm::vec2 lastMousePos = glm::vec2(-1, -1);
        float pitch = 0;
        float yaw = -90.0f;
        bool changed = false;

        glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, 0.0f);
        glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

        glm::mat4 inverseView = glm::mat4(1.0f);
        glm::mat4 inverseProjection = glm::mat4(1.0f);
    };
}

#endif //REINA_VK_CAMERA_H
