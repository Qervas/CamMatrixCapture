#include "core/camera_manager.hpp"
#include "core/sapera/sapera_camera.hpp"
#include "core/sapera_defs.hpp"
#include <QDebug>

namespace cam_matrix::core {

CameraManager::CameraManager(QObject* parent)
    : QObject(parent)
{
}

CameraManager::~CameraManager() = default;

void CameraManager::scanForCameras()
{
    cameras_.clear();

#if HAS_SAPERA
    // Check if Sapera SDK is available
    if (SaperaUtils::isSaperaAvailable()) {
        // Get the Sapera version
        std::string version = SaperaUtils::getSaperaVersion();
        emit statusChanged("Sapera SDK version: " + version);
        
        // Scan for Sapera cameras
        std::vector<std::string> cameraNames;
        if (SaperaUtils::getAvailableCameras(cameraNames)) {
            for (const auto& name : cameraNames) {
                cameras_.push_back(std::make_unique<sapera::SaperaCamera>(name));
            }
            emit statusChanged("Found " + std::to_string(cameras_.size()) + " Sapera cameras");
        } else {
            emit statusChanged("No Sapera cameras found");
        }
    } else {
        emit statusChanged("Sapera SDK not initialized properly");
    }
#elif HAS_GIGE_VISION
    // Using GigE Vision Interface
    std::string version = SaperaUtils::getGigeVisionVersion();
    emit statusChanged("GigE Vision Interface: " + version);
    
    // Scan for GigE Vision cameras
    std::vector<std::string> cameraNames;
    if (SaperaUtils::getAvailableCameras(cameraNames)) {
        for (const auto& name : cameraNames) {
            cameras_.push_back(std::make_unique<sapera::SaperaCamera>(name));
        }
        emit statusChanged("Found " + std::to_string(cameras_.size()) + " GigE Vision cameras");
    } else {
        // Add at least a mock camera for interface testing
        cameras_.push_back(std::make_unique<sapera::SaperaCamera>("Mock Camera"));
        emit statusChanged("No GigE Vision cameras found, added mock camera");
    }
#else
    emit statusChanged("Camera SDK not available");
    
    // Add dummy camera for UI testing
    cameras_.push_back(std::make_unique<sapera::SaperaCamera>("Dummy Camera"));
    emit statusChanged("Added dummy camera for testing");
#endif
}

std::vector<Camera*> CameraManager::getCameras() const
{
    std::vector<Camera*> result;
    result.reserve(cameras_.size());
    for (const auto& camera : cameras_) {
        result.push_back(camera.get());
    }
    return result;
}

Camera* CameraManager::getCameraByIndex(size_t index) const
{
    if (index < cameras_.size()) {
        return cameras_[index].get();
    }
    return nullptr;
}

sapera::SaperaCamera* CameraManager::getSaperaCameraByIndex(size_t index) const
{
    if (index < cameras_.size()) {
        return dynamic_cast<sapera::SaperaCamera*>(cameras_[index].get());
    }
    return nullptr;
}

bool CameraManager::connectCamera(size_t index)
{
    if (index < cameras_.size()) {
        bool result = cameras_[index]->connectCamera();
        if (result) {
            emit cameraConnected(index);
        }
        return result;
    }
    return false;
}

bool CameraManager::disconnectCamera(size_t index)
{
    if (index < cameras_.size()) {
        bool result = cameras_[index]->disconnectCamera();
        if (result) {
            emit cameraDisconnected(index);
        }
        return result;
    }
    return false;
}

bool CameraManager::disconnectAllCameras()
{
    bool allDisconnected = true;
    for (size_t i = 0; i < cameras_.size(); ++i) {
        if (!disconnectCamera(i)) {
            allDisconnected = false;
        }
    }
    return allDisconnected;
}

} // namespace cam_matrix::core
