#pragma once

#include "core/camera_manager.hpp"
#include <memory>
#include <vector>
#include <QWidget>
#include <QString>

namespace cam_matrix::core {

class CameraTester : public QWidget {
    Q_OBJECT

public:
    explicit CameraTester(QWidget* parent = nullptr);
    ~CameraTester();

    void initialize();
    void cleanup();

    void scanForCameras();
    void connectCamera(size_t index);
    void disconnectCamera(size_t index);
    void disconnectAllCameras();

    std::vector<Camera*> getCameras() const;
    Camera* getCameraByIndex(size_t index) const;
    sapera::SaperaCamera* getSaperaCameraByIndex(size_t index) const;

signals:
    void statusChanged(const QString& status);
    void error(const QString& error);
    void cameraConnected(size_t index);
    void cameraDisconnected(size_t index);

private:
    void setupUi();
    std::unique_ptr<CameraManager> cameraManager_;
};

} // namespace cam_matrix::core
