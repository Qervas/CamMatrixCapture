#pragma once

#include <memory>
#include <string>
#include <functional>
#include <opencv2/opencv.hpp>

class GUIManager {
public:
    GUIManager();
    ~GUIManager();

    // Initialize GLFW window
    bool initialize(const std::string& title, int width, int height);
    
    // Main loop
    void run();
    
    // Set camera preview callback
    void setPreviewCallback(std::function<void(const std::vector<cv::Mat>&)> callback);
    
    // Set capture callback
    void setCaptureCallback(std::function<void()> callback);
    
    // Update camera previews
    void updatePreviews(const std::vector<cv::Mat>& images);
    
    // Get window dimensions
    int getWidth() const;
    int getHeight() const;

private:
    struct WindowImpl;
    std::unique_ptr<WindowImpl> window;
    
    // Prevent copying
    GUIManager(const GUIManager&) = delete;
    GUIManager& operator=(const GUIManager&) = delete;
}; 