#include "FilesView.hpp"
#include <imgui.h>

namespace SaperaCapturePro {

FilesView::FilesView(SessionManager* session_mgr)
    : session_manager_(session_mgr)
{
    file_explorer_ = std::make_unique<FileExplorerWidget>();
    file_explorer_->Initialize();
}

FilesView::~FilesView() = default;

void FilesView::Render() {
    if (!session_manager_) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "❌ System not initialized");
        return;
    }

    CaptureSession* current_session = session_manager_->GetCurrentSession();

    if (!current_session) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "No active session - start a session to view files");
        return;
    }

    // Render file explorer content (without window wrapper)
    file_explorer_->RenderContent(current_session);
}

} // namespace SaperaCapturePro
