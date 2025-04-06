#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

reina::graphics::Camera::Camera(const reina::window::Window& window, float fov, float aspectRatio, glm::vec3 pos, glm::vec3 cameraFront)
        : renderWindow(&window), cameraPos(pos), cameraFront(cameraFront) {

    lastMousePos = glm::vec2(static_cast<float>(window.getWidth()) / 2.f, static_cast<float>(window.getHeight()) / 2.f);
    inverseProjection = glm::inverse(glm::perspective(fov, aspectRatio, 0.1f, 100.0f));
    inverseView = glm::inverse(glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp));

    // Register this Camera instance with the GLFW window
    GLFWwindow* glfwWin = window.getGlfwWindow();
    glfwSetWindowUserPointer(glfwWin, this);
    glfwSetCursorPosCallback(glfwWin, mouseCallback);
    glfwSetKeyCallback(glfwWin, keyCallback);

    pitch = static_cast<float>(glm::degrees(asin(cameraFront.y)));
    yaw = static_cast<float>(glm::degrees(atan2(cameraFront.z, cameraFront.x)));
}

reina::graphics::Camera::Camera(const Camera& other)
        : renderWindow(other.renderWindow),
          ignoreNextMouseInput(other.ignoreNextMouseInput),
          input(other.input),
          lastMousePos(other.lastMousePos),
          pitch(other.pitch),
          yaw(other.yaw),
          changed(other.changed),
          cameraPos(other.cameraPos),
          cameraFront(other.cameraFront),
          cameraUp(other.cameraUp),
          inverseView(other.inverseView),
          inverseProjection(other.inverseProjection)
{
    // Update the GLFW window's user pointer to point to this new copy.
    if (renderWindow) {
        glfwSetWindowUserPointer(renderWindow->getGlfwWindow(), this);
    }
}

reina::graphics::Camera& reina::graphics::Camera::operator=(const Camera& other) {
    if (this != &other) {
        renderWindow = other.renderWindow;
        ignoreNextMouseInput = other.ignoreNextMouseInput;
        input = other.input;
        lastMousePos = other.lastMousePos;
        pitch = other.pitch;
        yaw = other.yaw;
        changed = other.changed;
        cameraPos = other.cameraPos;
        cameraFront = other.cameraFront;
        cameraUp = other.cameraUp;
        inverseView = other.inverseView;
        inverseProjection = other.inverseProjection;

        if (renderWindow) {
            glfwSetWindowUserPointer(renderWindow->getGlfwWindow(), this);
        }
    }
    return *this;
}

void reina::graphics::Camera::processInput(const reina::window::Window& window, double timeDelta) {
    if (!input) {
        return;
    }

    const float cameraSpeed = 2.5f * static_cast<float>(timeDelta);

    if (window.keyPressed(GLFW_KEY_W)) {
        changed = true;
        cameraPos += cameraSpeed * cameraFront;
    } if (window.keyPressed(GLFW_KEY_S)) {
        changed = true;
        cameraPos -= cameraSpeed * cameraFront;
    } if (window.keyPressed(GLFW_KEY_A)) {
        changed = true;
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    } if (window.keyPressed(GLFW_KEY_D)) {
        changed = true;
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    } if (window.keyPressed(GLFW_KEY_SPACE)) {
        changed = true;
        cameraPos += cameraSpeed * cameraUp;
    } if (window.keyPressed(GLFW_KEY_LEFT_CONTROL)) {
        changed = true;
        cameraPos -= cameraSpeed * cameraUp;
    }
}

void reina::graphics::Camera::refresh() {
    if (changed) {
        changed = false;
        inverseView = glm::inverse(glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp));
    }
}

const glm::mat4& reina::graphics::Camera::getInverseView() const {
    return inverseView;
}

const glm::mat4& reina::graphics::Camera::getInverseProjection() const {
    return inverseProjection;
}

void reina::graphics::Camera::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    auto* cam = static_cast<Camera*>(glfwGetWindowUserPointer(window));

    if (!cam->input) {
        return;
    }

    if (cam->ignoreNextMouseInput) {
        cam->ignoreNextMouseInput = false;

        cam->lastMousePos.x = static_cast<float>(xpos);
        cam->lastMousePos.y = static_cast<float>(ypos);
        return;
    }

    auto xOffset = static_cast<float>(xpos - cam->lastMousePos.x);
    auto yOffset = -1 * static_cast<float>(ypos - cam->lastMousePos.y);

    cam->lastMousePos.x = static_cast<float>(xpos);
    cam->lastMousePos.y = static_cast<float>(ypos);

    cam->changed = true;

    const float sensitivity = 0.05f;
    xOffset *= sensitivity;
    yOffset *= sensitivity;

    cam->yaw += xOffset;
    cam->pitch += yOffset;

    if (cam->pitch > 89.0f) {
        cam->pitch = 89.0f;
    } else if (cam->pitch < -89.0f) {
        cam->pitch = -89.0f;
    }

    cam->cameraFront = glm::normalize(glm::vec3{
        static_cast<float>(cos(glm::radians(cam->yaw)) * cos(glm::radians(cam->pitch))),
        static_cast<float>(sin(glm::radians(cam->pitch))),
        static_cast<float>(sin(glm::radians(cam->yaw)) * cos(glm::radians(cam->pitch)))
    });
}

void reina::graphics::Camera::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto* cam = static_cast<Camera*>(glfwGetWindowUserPointer(window));

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        cam->toggleInput(window);
    }
}

bool reina::graphics::Camera::hasChanged() const {
    return changed;
}

void reina::graphics::Camera::toggleInput(GLFWwindow* window) {
    input = !input;
    glfwSetInputMode(window, GLFW_CURSOR, input ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    ignoreNextMouseInput = true;
}

bool reina::graphics::Camera::isAcceptingInput() const {
    return input;
}
