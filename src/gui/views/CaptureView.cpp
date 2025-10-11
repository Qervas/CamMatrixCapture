#include "CaptureView.hpp"
#include "../widgets/CaptureStudioPanel.hpp"

namespace SaperaCapturePro {

CaptureView::CaptureView(CameraManager* camera_mgr, SessionManager* session_mgr, BluetoothManager* bt_mgr, SettingsManager* settings_mgr) {
    capture_panel_ = std::make_unique<CaptureStudioPanel>();
    capture_panel_->Initialize(camera_mgr, bt_mgr, session_mgr, settings_mgr);
}

CaptureView::~CaptureView() {
    if (capture_panel_) {
        capture_panel_->Shutdown();
    }
}

void CaptureView::Render() {
    if (capture_panel_) {
        capture_panel_->RenderContent();
    }
}

} // namespace SaperaCapturePro
