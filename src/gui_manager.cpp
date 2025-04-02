#include "gui_manager.hpp"
#include <GLFW/glfw3.h>
#include <opencv2/opencv.hpp>
#include <stdexcept>
#include <iostream>

struct GUIManager::WindowImpl {
    GLFWwindow* window = nullptr;
    int width = 0;
    int height = 0;
    std::function<void(const std::vector<cv::Mat>&)> previewCallback;
    std::function<void()> captureCallback;
    std::vector<cv::Mat> currentPreviews;
};

GUIManager::GUIManager() : window(std::make_unique<WindowImpl>()) {
    std::cout << "GUIManager constructor called" << std::endl;
}

GUIManager::~GUIManager() {
    std::cout << "GUIManager destructor called" << std::endl;
    if (window->window) {
        std::cout << "Destroying GLFW window..." << std::endl;
        glfwDestroyWindow(window->window);
    }
    std::cout << "Terminating GLFW..." << std::endl;
    glfwTerminate();
}

bool GUIManager::initialize(const std::string& title, int width, int height) {
    std::cout << "Initializing GUI manager..." << std::endl;
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }
    std::cout << "GLFW initialized successfully" << std::endl;

    window->width = width;
    window->height = height;

    // Create window
    std::cout << "Creating GLFW window..." << std::endl;
    window->window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window->window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }
    std::cout << "GLFW window created successfully" << std::endl;

    glfwMakeContextCurrent(window->window);
    std::cout << "OpenGL context made current" << std::endl;

    // Set up key callback
    glfwSetKeyCallback(window->window, [](GLFWwindow* w, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            std::cout << "ESC key pressed, closing window..." << std::endl;
            glfwSetWindowShouldClose(w, GLFW_TRUE);
        }
    });
    std::cout << "Key callback set up" << std::endl;

    return true;
}

void GUIManager::run() {
    std::cout << "Starting main GUI loop..." << std::endl;
    while (!glfwWindowShouldClose(window->window)) {
        glfwPollEvents();

        // Clear the screen
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw camera previews if available
        if (!window->currentPreviews.empty()) {
            std::cout << "Drawing " << window->currentPreviews.size() << " camera previews..." << std::endl;
            // TODO: Implement OpenGL texture rendering for camera previews
            // This will require converting OpenCV images to OpenGL textures
        }

        // Swap buffers
        glfwSwapBuffers(window->window);
    }
    std::cout << "Main GUI loop ended" << std::endl;
}

void GUIManager::setPreviewCallback(std::function<void(const std::vector<cv::Mat>&)> callback) {
    std::cout << "Setting preview callback..." << std::endl;
    window->previewCallback = std::move(callback);
    std::cout << "Preview callback set" << std::endl;
}

void GUIManager::setCaptureCallback(std::function<void()> callback) {
    std::cout << "Setting capture callback..." << std::endl;
    window->captureCallback = std::move(callback);
    std::cout << "Capture callback set" << std::endl;
}

void GUIManager::updatePreviews(const std::vector<cv::Mat>& images) {
    std::cout << "Updating previews with " << images.size() << " images..." << std::endl;
    window->currentPreviews = images;
    std::cout << "Previews updated" << std::endl;
}

int GUIManager::getWidth() const {
    return window->width;
}

int GUIManager::getHeight() const {
    return window->height;
} 