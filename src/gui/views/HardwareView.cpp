#include "HardwareView.hpp"

namespace SaperaCapturePro {

HardwareView::HardwareView(BluetoothManager* bt_mgr, CameraManager* camera_mgr,
                           SessionManager* session_mgr, SettingsManager* settings_mgr) {
    hardware_panel_ = std::make_unique<HardwarePanel>();
    hardware_panel_->Initialize(bt_mgr, camera_mgr, session_mgr, settings_mgr);
}

HardwareView::~HardwareView() = default;

void HardwareView::Render() {
    if (hardware_panel_) {
        hardware_panel_->RenderContent();
    }
}

} // namespace SaperaCapturePro
