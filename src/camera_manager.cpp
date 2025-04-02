#include "camera_manager.hpp"
#include <opencv2/opencv.hpp>
#include <stdexcept>
#include <iostream>

struct CameraManager::CameraImpl {
    cv::VideoCapture cap;
    bool isOpen = false;
};

CameraManager::CameraManager() : resolution(1920, 1080) {
    std::cout << "CameraManager constructor called" << std::endl;
}

CameraManager::~CameraManager() {
    std::cout << "CameraManager destructor called" << std::endl;
    cameras.clear();
}

bool CameraManager::initializeCameras(int numCameras) {
    std::cout << "Initializing " << numCameras << " cameras..." << std::endl;
    cameras.clear();
    cameras.reserve(numCameras);

    for (int i = 0; i < numCameras; ++i) {
        std::cout << "Opening camera " << i << "..." << std::endl;
        auto camera = std::make_unique<CameraImpl>();
        
        // Open camera
        if (!camera->cap.open(i)) {
            std::cerr << "Failed to open camera " << i << std::endl;
            return false;
        }

        // Set camera properties
        std::cout << "Setting camera " << i << " properties..." << std::endl;
        camera->cap.set(cv::CAP_PROP_FRAME_WIDTH, resolution.width);
        camera->cap.set(cv::CAP_PROP_FRAME_HEIGHT, resolution.height);
        camera->cap.set(cv::CAP_PROP_FPS, 30.0);
        
        camera->isOpen = true;
        cameras.push_back(std::move(camera));
        std::cout << "Camera " << i << " initialized successfully" << std::endl;
    }

    std::cout << "All cameras initialized successfully" << std::endl;
    return true;
}

bool CameraManager::captureAll(std::vector<cv::Mat>& images) {
    std::cout << "Starting capture from all cameras..." << std::endl;
    images.clear();
    images.reserve(cameras.size());

    // First, prepare all cameras
    for (size_t i = 0; i < cameras.size(); ++i) {
        if (!cameras[i]->isOpen) {
            std::cerr << "Camera " << i << " is not open" << std::endl;
            return false;
        }
    }

    // Then capture from all cameras
    for (size_t i = 0; i < cameras.size(); ++i) {
        std::cout << "Capturing from camera " << i << "..." << std::endl;
        cv::Mat frame;
        if (!cameras[i]->cap.read(frame)) {
            std::cerr << "Failed to read from camera " << i << std::endl;
            return false;
        }
        images.push_back(frame.clone());
        std::cout << "Successfully captured from camera " << i << std::endl;
    }

    std::cout << "Successfully captured from all cameras" << std::endl;
    return true;
}

int CameraManager::getNumCameras() const {
    return static_cast<int>(cameras.size());
}

cv::Size CameraManager::getResolution() const {
    return resolution;
}

bool CameraManager::setExposure(double exposure) {
    std::cout << "Setting exposure to " << exposure << "..." << std::endl;
    for (size_t i = 0; i < cameras.size(); ++i) {
        if (!cameras[i]->cap.set(cv::CAP_PROP_EXPOSURE, exposure)) {
            std::cerr << "Failed to set exposure for camera " << i << std::endl;
            return false;
        }
    }
    std::cout << "Successfully set exposure for all cameras" << std::endl;
    return true;
}

bool CameraManager::setGain(double gain) {
    std::cout << "Setting gain to " << gain << "..." << std::endl;
    for (size_t i = 0; i < cameras.size(); ++i) {
        if (!cameras[i]->cap.set(cv::CAP_PROP_GAIN, gain)) {
            std::cerr << "Failed to set gain for camera " << i << std::endl;
            return false;
        }
    }
    std::cout << "Successfully set gain for all cameras" << std::endl;
    return true;
}

bool CameraManager::setFrameRate(double fps) {
    std::cout << "Setting frame rate to " << fps << "..." << std::endl;
    for (size_t i = 0; i < cameras.size(); ++i) {
        if (!cameras[i]->cap.set(cv::CAP_PROP_FPS, fps)) {
            std::cerr << "Failed to set frame rate for camera " << i << std::endl;
            return false;
        }
    }
    std::cout << "Successfully set frame rate for all cameras" << std::endl;
    return true;
} 