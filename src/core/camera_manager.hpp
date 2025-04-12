#pragma once
#include "camera.hpp"
#include "camera/mock_camera.hpp"
#include <vector>
#include <memory>
#include <string>
#include <QObject>

namespace cam_matrix::core {

class CameraManager : public QObject {
    Q_OBJECT

public:
    CameraManager();
    virtual ~CameraManager();

    // Scan for available cameras
    virtual bool scanForCameras();

    // Camera access
    std::vector<std::shared_ptr<Camera>> getCameras() const;
    std::shared_ptr<Camera> getCameraByIndex(size_t index) const;
    std::shared_ptr<MockCamera> getMockCameraByIndex(size_t index) const;

    // Connection management
    bool connectCamera(size_t index);
    bool disconnectCamera(size_t index);
    bool disconnectAllCameras();

signals:
    void camerasChanged();
    void cameraConnected(size_t index);
    void cameraDisconnected(size_t index);

protected:
    std::vector<std::shared_ptr<Camera>> cameras_;
};

} // namespace cam_matrix::core
