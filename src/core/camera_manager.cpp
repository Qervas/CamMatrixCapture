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
    // Automatically scan for cameras on initialization
    QMetaObject::invokeMethod(this, "scanForCameras", Qt::QueuedConnection);
}

CameraManager::~CameraManager() = default;

void CameraManager::scanForCameras()
{
    cameras_.clear();
    emit managerStatusChanged("Scanning for cameras...");

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
            
            // No need to try direct access - it will be handled by the dialog if needed
        } else {
            emit statusChanged("No Sapera cameras found");
            // No longer adding mock cameras for testing
        }
    } else {
        emit statusChanged("Sapera SDK not initialized properly");
        // No longer adding mock cameras for testing
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
        // No longer adding mock cameras for interface testing
        emit statusChanged("No GigE Vision cameras found");
    }
#else
    emit statusChanged("Camera SDK not available");
    // No longer adding dummy cameras for UI testing
#endif

    // Notify the UI that camera list has been updated
    emit cameraStatusChanged("Camera list updated");
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
        qDebug() << "Connecting camera at index" << index << "..." << QString::fromStdString(cameras_[index]->getName());
        
        // First disconnect any existing connections to prevent duplicates
        disconnect(cameras_[index].get(), nullptr, this, nullptr);
        
        bool result = cameras_[index]->connectCamera();
        if (result) {
            qDebug() << "Camera connected successfully, setting up signal/slot connections";
            
            // Connect the camera's newFrameAvailable signal to our own
            // Important: Use Qt::QueuedConnection to ensure thread safety
            connect(cameras_[index].get(), &Camera::newFrameAvailable,
                    this, &CameraManager::newFrameAvailable, Qt::QueuedConnection);
            
            // Debug: Check if the camera can provide frames
            auto* saperaCam = dynamic_cast<sapera::SaperaCamera*>(cameras_[index].get());
            if (saperaCam) {
                QImage currentFrame = saperaCam->getFrame();
                if (!currentFrame.isNull()) {
                    qDebug() << "Initial frame received from camera, size:" << currentFrame.size();
                    // Forward this initial frame
                    emit newFrameAvailable(currentFrame);
                } else {
                    qDebug() << "Initial frame is null";
                }
            }
            
            emit cameraConnected(index);
            emit cameraStatusChanged(QString("Camera %1 connected").arg(QString::fromStdString(cameras_[index]->getName())));
        } else {
            qDebug() << "Failed to connect camera at index" << index;
            emit cameraStatusChanged(QString("Failed to connect camera %1").arg(QString::fromStdString(cameras_[index]->getName())));
        }
        return result;
    }
    return false;
}

bool CameraManager::disconnectCamera(size_t index)
{
    if (index < cameras_.size()) {
        // Disconnect the signal connection before disconnecting the camera
        disconnect(cameras_[index].get(), &Camera::newFrameAvailable,
                   this, &CameraManager::newFrameAvailable);
        
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

// New method implementations
std::vector<std::string> CameraManager::getAvailableCameras() const {
    std::vector<std::string> cameraNames;
    cameraNames.reserve(cameras_.size());
    
    for (const auto& camera : cameras_) {
        cameraNames.push_back(camera->getName());
    }
    
    return cameraNames;
}

bool CameraManager::isCameraConnected(size_t index) const {
    if (index < cameras_.size()) {
        return cameras_[index]->isConnected();
    }
    return false;
}

bool CameraManager::setExposureTime(size_t index, double value) {
    if (index < cameras_.size()) {
        bool result = cameras_[index]->setExposureTime(value);
        if (result) {
            emit managerStatusChanged("Exposure time set to " + std::to_string(value) + 
                                     " for camera " + std::to_string(index));
        }
        return result;
    }
    return false;
}

bool CameraManager::setGain(size_t index, double value) {
    if (index < cameras_.size()) {
        // Assuming Camera class would have a setGain method
        // This might need to be implemented in the Camera base class
        auto* saperaCam = dynamic_cast<sapera::SaperaCamera*>(cameras_[index].get());
        if (saperaCam) {
            bool result = saperaCam->setGain(value);
            if (result) {
                emit managerStatusChanged("Gain set to " + std::to_string(value) + 
                                         " for camera " + std::to_string(index));
            }
            return result;
        }
    }
    return false;
}

bool CameraManager::setFormat(size_t index, const std::string& format) {
    if (index < cameras_.size()) {
        // Assuming Camera class would have a setFormat method
        // This might need to be implemented in the Camera base class
        auto* saperaCam = dynamic_cast<sapera::SaperaCamera*>(cameras_[index].get());
        if (saperaCam) {
            bool result = saperaCam->setPixelFormat(format);
            if (result) {
                emit managerStatusChanged("Format set to " + format + 
                                         " for camera " + std::to_string(index));
            }
            return result;
        }
    }
    return false;
}

double CameraManager::getExposureTime(size_t index) const {
    if (index < cameras_.size()) {
        auto* saperaCam = dynamic_cast<sapera::SaperaCamera*>(cameras_[index].get());
        if (saperaCam) {
            return saperaCam->getExposureTime();
        }
    }
    return 0.0;
}

double CameraManager::getGain(size_t index) const {
    if (index < cameras_.size()) {
        auto* saperaCam = dynamic_cast<sapera::SaperaCamera*>(cameras_[index].get());
        if (saperaCam) {
            return saperaCam->getGain();
        }
    }
    return 0.0;
}

std::string CameraManager::getFormat(size_t index) const {
    if (index < cameras_.size()) {
        auto* saperaCam = dynamic_cast<sapera::SaperaCamera*>(cameras_[index].get());
        if (saperaCam) {
            return saperaCam->getPixelFormat();
        }
    }
    return "Unknown";
}

bool CameraManager::capturePhoto(size_t index, const std::string& path) {
    if (index < cameras_.size() && cameras_[index]->isConnected()) {
        bool result = cameras_[index]->capturePhoto(path);
        if (result) {
            // This is a placeholder. In a real implementation, we'd need to get the 
            // image from the camera and emit it alongside the path
            emit managerStatusChanged("Photo captured from camera " + std::to_string(index) + 
                                     " and saved to " + path);
            
            // For the signal that camera_page.cpp expects, we would need to emit the actual image
            // This is just a placeholder as we don't have access to the actual image here
            QImage dummyImage;
            emit photoCaptured(dummyImage, path);
        }
        return result;
    }
    return false;
}

void CameraManager::updateCamerasFromDirectAccess(const std::vector<std::string>& cameraNames) {
    if (cameraNames.empty()) {
        emit managerStatusChanged("No cameras found from direct access");
        return;
    }
    
    bool cameraAdded = false;
    
    // Check which cameras are not already in our list
    std::vector<std::string> existingNames = getAvailableCameras();
    
    // Track indices where we find matches
    std::set<size_t> existingIndices;
    
    // Check each camera from direct access against existing cameras
    for (const auto& directName : cameraNames) {
        bool found = false;
        
        // See if this camera name already exists in our list
        for (size_t i = 0; i < existingNames.size(); ++i) {
            if (existingNames[i] == directName) {
                found = true;
                existingIndices.insert(i);
                break;
            }
        }
        
        // If not found, add it to our camera list
        if (!found) {
            cameras_.push_back(std::make_unique<sapera::SaperaCamera>(directName));
            emit managerStatusChanged("Added camera from direct access: " + directName);
            cameraAdded = true;
        }
    }
    
    if (cameraAdded) {
        emit cameraStatusChanged("Cameras updated from direct access");
    } else {
        emit managerStatusChanged("No new cameras found from direct access");
    }
}

} // namespace cam_matrix::core
