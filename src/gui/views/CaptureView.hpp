#pragma once

#include <memory>
#include "../../hardware/CameraManager.hpp"
#include "../../utils/SessionManager.hpp"
#include "../../utils/SettingsManager.hpp"
#include "../../bluetooth/BluetoothManager.hpp"

namespace SaperaCapturePro {

class CaptureStudioPanel;  // Forward declaration

class CaptureView {
public:
    CaptureView(CameraManager* camera_mgr, SessionManager* session_mgr, BluetoothManager* bt_mgr, SettingsManager* settings_mgr);
    ~CaptureView();

    void Render();

private:
    std::unique_ptr<CaptureStudioPanel> capture_panel_;
};

} // namespace SaperaCapturePro
