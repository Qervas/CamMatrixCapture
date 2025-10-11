#pragma once

#include <memory>
#include "../HardwarePanel.hpp"
#include "../../hardware/CameraManager.hpp"
#include "../../bluetooth/BluetoothManager.hpp"
#include "../../utils/SessionManager.hpp"
#include "../../utils/SettingsManager.hpp"

namespace SaperaCapturePro {

class HardwareView {
public:
    HardwareView(BluetoothManager* bt_mgr, CameraManager* camera_mgr,
                 SessionManager* session_mgr, SettingsManager* settings_mgr);
    ~HardwareView();

    void Render();

private:
    std::unique_ptr<HardwarePanel> hardware_panel_;
};

} // namespace SaperaCapturePro
