#pragma once

#include <functional>
#include <vector>
#include <string>

struct CameraInfo;

class CameraControlPanel {
public:
    CameraControlPanel();
    ~CameraControlPanel();

    void Initialize();
    void Render(const std::vector<CameraInfo>& cameras);

    // Callbacks
    std::function<void()> onDiscoverCameras;
    std::function<void()> onConnectAllCameras;
    std::function<void()> onDisconnectAllCameras;
    std::function<void(int)> onConnectCamera;
    std::function<void(int)> onDisconnectCamera;

    bool visible = true;

private:
    // UI state
    bool auto_connect_ = false;
    int selected_camera_ = -1;
    char camera_filter_[256] = "";
    
    void RenderCameraTable(const std::vector<CameraInfo>& cameras);
    void RenderCameraDetails(const CameraInfo& camera);
    void RenderConnectionControls();
}; 