#pragma once

#include <vector>
#include <memory>
#include <opencv2/opencv.hpp>

class CameraManager {
public:
    CameraManager();
    ~CameraManager();

    // Initialize camera matrix
    bool initializeCameras(int numCameras);
    
    // Synchronous capture from all cameras
    bool captureAll(std::vector<cv::Mat>& images);
    
    // Get number of connected cameras
    int getNumCameras() const;
    
    // Get camera resolution
    cv::Size getResolution() const;
    
    // Set camera parameters
    bool setExposure(double exposure);
    bool setGain(double gain);
    bool setFrameRate(double fps);

private:
    struct CameraImpl;
    std::vector<std::unique_ptr<CameraImpl>> cameras;
    cv::Size resolution;
    
    // Prevent copying
    CameraManager(const CameraManager&) = delete;
    CameraManager& operator=(const CameraManager&) = delete;
}; 