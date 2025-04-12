#pragma once
#include "core/camera.hpp"
#include "core/sapera_camera.hpp"
#include "core/sapera_defs.hpp"
#include "core/camera_manager.hpp"

#include <vector>
#include <memory>
#include <string>
#include <QObject>

namespace cam_matrix::core {

class SaperaManager : public CameraManager{
    Q_OBJECT

public:
    SaperaManager();
    ~SaperaManager();

    // Camera discovery
    bool scanForCameras();
    std::vector<std::shared_ptr<Camera>> getCameras() const;
    std::shared_ptr<Camera> getCameraByIndex(size_t index) const;
    std::shared_ptr<SaperaCamera> getSaperaCameraByIndex(size_t index) const;

    // Check if Sapera is installed
    static bool isSaperaInstalled();

signals:
    void camerasChanged();
    void statusChanged(const std::string& message);
    void error(const std::string& message);

private:
    std::vector<std::shared_ptr<SaperaCamera>> cameras_;
};

} // namespace cam_matrix::core
