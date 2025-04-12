#include "core/camera_manager.hpp"
#include <iostream>

namespace cam_matrix::core {

CameraManager::CameraManager() {
    // Initial scan for cameras
    scanForCameras();
}

CameraManager::~CameraManager() {
    disconnectAllCameras();
}

bool CameraManager::scanForCameras() {
    disconnectAllCameras();
    cameras_.clear();

    // Create some mock cameras
    for (int i = 0; i < 3; ++i) {
        std::string cameraName = "Mock Camera " + std::to_string(i);
        cameras_.push_back(std::make_shared<MockCamera>(i, cameraName));
    }

    emit camerasChanged();
    return true;
}

std::vector<std::shared_ptr<Camera>> CameraManager::getCameras() const {
    return cameras_; // Now directly returns since we store Camera pointers
}

std::shared_ptr<Camera> CameraManager::getCameraByIndex(size_t index) const {
    if (index < cameras_.size()) {
        return cameras_[index];
    }
    return nullptr;
}

std::shared_ptr<MockCamera> CameraManager::getMockCameraByIndex(size_t index) const {
    if (index < cameras_.size()) {
        return std::dynamic_pointer_cast<MockCamera>(cameras_[index]);
    }
    return nullptr;
}

bool CameraManager::connectCamera(size_t index) {
    if (index < cameras_.size()) {
        bool result = cameras_[index]->connectCamera();
        if (result) {
            emit cameraConnected(index);
        }
        return result;
    }
    return false;
}

bool CameraManager::disconnectCamera(size_t index) {
    if (index < cameras_.size()) {
        bool result = cameras_[index]->disconnectCamera();
        if (result) {
            emit cameraDisconnected(index);
        }
        return result;
    }
    return false;
}

bool CameraManager::disconnectAllCameras() {
    bool allDisconnected = true;
    for (size_t i = 0; i < cameras_.size(); ++i) {
        if (!disconnectCamera(i)) {
            allDisconnected = false;
        }
    }
    return allDisconnected;
}

} // namespace cam_matrix::core
