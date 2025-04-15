#pragma once

#include "core/camera.hpp"
#include "core/sapera/sapera_camera.hpp"
#include <memory>
#include <vector>
#include <QObject>
#include <set>
#include <QDateTime>
#include <QDir>
#include <future>
#include <mutex>

namespace cam_matrix::core {

class CameraManager : public QObject {
    Q_OBJECT

public:
    explicit CameraManager(QObject* parent = nullptr);
    ~CameraManager();

    // Scan for available cameras
    void scanForCameras();

    // Get list of available cameras
    std::vector<Camera*> getCameras() const;

    // Camera access
    Camera* getCameraByIndex(size_t index) const;
    sapera::SaperaCamera* getSaperaCameraByIndex(size_t index) const;

    // Connection management
    bool connectCamera(size_t index);
    bool disconnectCamera(size_t index);
    bool disconnectAllCameras();

    // Multi-camera management
    void selectCameraForSync(size_t index, bool selected);
    void clearCameraSelection();
    bool connectSelectedCameras();
    bool disconnectSelectedCameras();
    std::set<size_t> getSelectedCameras() const;
    
    // Synchronized capture
    bool capturePhotosSync(const std::string& basePath = "");
    
    // Check if camera is selected for sync
    bool isCameraSelected(size_t index) const;

signals:
    void statusChanged(const std::string& status);
    void error(const std::string& error);
    void cameraConnected(size_t index);
    void cameraDisconnected(size_t index);
    void syncCaptureStarted(int count);
    void syncCaptureComplete(int successCount, int totalCount);
    void syncCaptureProgress(int current, int total);
    void photoCaptured(size_t cameraIndex, const std::string& path);

private:
    std::vector<std::unique_ptr<Camera>> cameras_;
    std::set<size_t> selectedCameras_;
    mutable std::mutex selectionMutex_;
    
    // Helper method for sync capture
    std::string generateCaptureFolder() const;
};

} // namespace cam_matrix::core
