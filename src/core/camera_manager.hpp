#pragma once

#include "core/camera.hpp"
#include "core/sapera_camera.hpp"
#include <memory>
#include <vector>
#include <QObject>

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
    SaperaCamera* getSaperaCameraByIndex(size_t index) const;

    // Connection management
    bool connectCamera(size_t index);
    bool disconnectCamera(size_t index);
    bool disconnectAllCameras();

signals:
    void statusChanged(const std::string& status);
    void error(const std::string& error);
    void cameraConnected(size_t index);
    void cameraDisconnected(size_t index);

private:
    std::vector<std::unique_ptr<Camera>> cameras_;
};

} // namespace cam_matrix::core
