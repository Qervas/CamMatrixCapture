#include "core/camera_manager.hpp"
#include "core/sapera/sapera_camera.hpp"
#include "core/sapera_defs.hpp"
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <thread>
#include <future>
#include <chrono>

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

// New multi-camera management methods
void CameraManager::selectCameraForSync(size_t index, bool selected)
{
    if (index >= cameras_.size()) {
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(selectionMutex_);
        if (selected) {
            selectedCameras_.insert(index);
        } else {
            selectedCameras_.erase(index);
        }
    }
    
    emit statusChanged("Camera " + std::to_string(index) + (selected ? " selected" : " deselected") + 
                      " for sync (" + std::to_string(selectedCameras_.size()) + " cameras selected)");
}

void CameraManager::clearCameraSelection()
{
    {
        std::lock_guard<std::mutex> lock(selectionMutex_);
        selectedCameras_.clear();
    }
    
    emit statusChanged("Camera selection cleared");
}

bool CameraManager::connectSelectedCameras()
{
    bool allConnected = true;
    std::vector<size_t> indexesToConnect;
    
    // Get copy of selection to avoid locking during connection
    {
        std::lock_guard<std::mutex> lock(selectionMutex_);
        indexesToConnect.assign(selectedCameras_.begin(), selectedCameras_.end());
    }
    
    if (indexesToConnect.empty()) {
        emit error("No cameras selected for synchronized connection");
        return false;
    }
    
    emit statusChanged("Connecting " + std::to_string(indexesToConnect.size()) + " cameras...");
    
    // Connect cameras sequentially - this could be parallelized in future
    int connectedCount = 0;
    for (size_t index : indexesToConnect) {
        if (index < cameras_.size()) {
            bool result = cameras_[index]->connectCamera();
            if (result) {
                emit cameraConnected(index);
                connectedCount++;
            } else {
                allConnected = false;
                emit error("Failed to connect camera " + std::to_string(index));
            }
        }
    }
    
    emit statusChanged("Connected " + std::to_string(connectedCount) + " of " + 
                      std::to_string(indexesToConnect.size()) + " cameras");
    
    return allConnected;
}

bool CameraManager::disconnectSelectedCameras()
{
    bool allDisconnected = true;
    std::vector<size_t> indexesToDisconnect;
    
    // Get copy of selection to avoid locking during disconnection
    {
        std::lock_guard<std::mutex> lock(selectionMutex_);
        indexesToDisconnect.assign(selectedCameras_.begin(), selectedCameras_.end());
    }
    
    if (indexesToDisconnect.empty()) {
        emit error("No cameras selected for synchronized disconnection");
        return false;
    }
    
    emit statusChanged("Disconnecting " + std::to_string(indexesToDisconnect.size()) + " cameras...");
    
    // Disconnect cameras sequentially
    int disconnectedCount = 0;
    for (size_t index : indexesToDisconnect) {
        if (index < cameras_.size()) {
            bool result = cameras_[index]->disconnectCamera();
            if (result) {
                emit cameraDisconnected(index);
                disconnectedCount++;
            } else {
                allDisconnected = false;
                emit error("Failed to disconnect camera " + std::to_string(index));
            }
        }
    }
    
    emit statusChanged("Disconnected " + std::to_string(disconnectedCount) + " of " + 
                      std::to_string(indexesToDisconnect.size()) + " cameras");
    
    return allDisconnected;
}

std::set<size_t> CameraManager::getSelectedCameras() const
{
    std::lock_guard<std::mutex> lock(selectionMutex_);
    return selectedCameras_;
}

bool CameraManager::isCameraSelected(size_t index) const
{
    std::lock_guard<std::mutex> lock(selectionMutex_);
    return selectedCameras_.find(index) != selectedCameras_.end();
}

std::string CameraManager::generateCaptureFolder() const
{
    // Create a timestamp-based folder name
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QString folderName = "captures/nerf_" + timestamp;
    
    // Create the directory
    QDir dir(folderName);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "Failed to create directory:" << folderName;
            return "";
        }
    }
    
    return folderName.toStdString();
}

bool CameraManager::capturePhotosSync(const std::string& basePath)
{
    std::vector<size_t> indexesToCapture;
    
    // Get copy of selection to avoid locking during capture
    {
        std::lock_guard<std::mutex> lock(selectionMutex_);
        indexesToCapture.assign(selectedCameras_.begin(), selectedCameras_.end());
    }
    
    if (indexesToCapture.empty()) {
        emit error("No cameras selected for synchronized capture");
        return false;
    }
    
    // Check if all selected cameras are connected
    for (size_t index : indexesToCapture) {
        if (index >= cameras_.size() || !cameras_[index]->isConnected()) {
            emit error("Camera " + std::to_string(index) + " is not connected. "
                      "All cameras must be connected for synchronized capture.");
            return false;
        }
    }
    
    // Generate a capture folder if not provided
    std::string captureFolder = basePath;
    if (captureFolder.empty()) {
        captureFolder = generateCaptureFolder();
        if (captureFolder.empty()) {
            emit error("Failed to create capture folder");
            return false;
        }
    }
    
    emit syncCaptureStarted(static_cast<int>(indexesToCapture.size()));
    emit statusChanged("Starting synchronized capture with " + 
                      std::to_string(indexesToCapture.size()) + " cameras...");
    
    // Prepare the cameras for capture (could include focusing, etc.)
    // For now, we'll just sleep briefly to ensure cameras are ready
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Use async to capture from all cameras simultaneously
    std::vector<std::future<bool>> captureFutures;
    std::vector<std::string> capturePaths;
    captureFutures.reserve(indexesToCapture.size());
    capturePaths.reserve(indexesToCapture.size());
    
    // Launch capture operations for all cameras
    for (size_t i = 0; i < indexesToCapture.size(); ++i) {
        size_t cameraIndex = indexesToCapture[i];
        
        // Generate timestamp for the filename
        QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss-zzz");
        
        // Create filename with camera index, name, and timestamp
        std::string filename = captureFolder + "/camera_" + 
                              std::to_string(cameraIndex) + "_" +
                              cameras_[cameraIndex]->getName() + "_" +
                              timeStamp.toStdString() + ".png";
        
        capturePaths.push_back(filename);
        
        // Launch async capture operation
        captureFutures.push_back(std::async(std::launch::async, [this, cameraIndex, filename]() {
            auto* camera = dynamic_cast<sapera::SaperaCamera*>(cameras_[cameraIndex].get());
            if (camera) {
                return camera->capturePhoto(filename);
            }
            return false;
        }));
    }
    
    // Wait for all captures to complete and count successes
    int successCount = 0;
    for (size_t i = 0; i < captureFutures.size(); ++i) {
        bool success = captureFutures[i].get();
        if (success) {
            successCount++;
            emit photoCaptured(indexesToCapture[i], capturePaths[i]);
        }
        emit syncCaptureProgress(static_cast<int>(i + 1), 
                               static_cast<int>(captureFutures.size()));
    }
    
    emit syncCaptureComplete(successCount, static_cast<int>(indexesToCapture.size()));
    emit statusChanged("Synchronized capture complete: " + std::to_string(successCount) + 
                      " of " + std::to_string(indexesToCapture.size()) + " successful");
    
    return successCount == static_cast<int>(indexesToCapture.size());
}

} // namespace cam_matrix::core
