#include "SessionWidget.hpp"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace SaperaCapturePro {

SessionWidget::SessionWidget() = default;

SessionWidget::~SessionWidget() {
    Shutdown();
}

void SessionWidget::Initialize(SessionManager* session_manager) {
    session_manager_ = session_manager;
    was_session_active_ = HasActiveSession();
}

void SessionWidget::Shutdown() {
    session_manager_ = nullptr;
}

void SessionWidget::Render() {
    if (!session_manager_) return;
    
    CheckSessionStateChanged();
    
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::BeginChild("SessionWidget", ImVec2(0, 80), true, ImGuiWindowFlags_NoScrollbar);
    
    ImGui::Text("📁 Session Control");
    ImGui::Separator();
    
    if (HasActiveSession()) {
        RenderActiveSession();
    } else {
        RenderSessionCreator();
    }
    
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void SessionWidget::RenderActiveSession() {
    auto* session = GetCurrentSession();
    if (!session) return;
    
    // Session info in compact layout
    ImGui::Columns(3, "session_cols", false);
    ImGui::SetColumnWidth(0, 200);
    ImGui::SetColumnWidth(1, 150);
    
    // Column 1: Session name and status
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ %s", session->object_name.c_str());
    ImGui::Text("ID: %s", session->session_id.substr(0, 8).c_str());
    
    ImGui::NextColumn();
    
    // Column 2: Stats
    ImGui::Text("📸 %d captures", session->capture_count);
    ImGui::Text("📂 %zu files", session->capture_paths.size());
    
    ImGui::NextColumn();
    
    // Column 3: Actions
    if (ImGui::Button("🗂️ Open Folder", ImVec2(-1, 0))) {
        std::filesystem::path abs_path = std::filesystem::absolute(session->base_path);
        std::string command = "explorer \"" + abs_path.string() + "\"";
        system(command.c_str());
        LogMessage("[SESSION] Opened session folder: " + abs_path.string());
    }
    
    if (ImGui::Button("⏹ End Session", ImVec2(-1, 0))) {
        session_manager_->EndCurrentSession();
        LogMessage("[SESSION] Session ended from session widget");
    }
    
    ImGui::Columns(1);
}

void SessionWidget::RenderSessionCreator() {
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "⚠ No active session");
    
    ImGui::Columns(2, "create_cols", false);
    ImGui::SetColumnWidth(0, 250);
    
    // Column 1: Input
    ImGui::Text("Object Name:");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##NewObjectName", "Enter object name or leave empty for auto-generated", 
                           new_object_name_, sizeof(new_object_name_));
    
    ImGui::NextColumn();
    
    // Column 2: Action
    ImGui::Spacing();
    if (ImGui::Button("▶ Start New Session", ImVec2(-1, 0))) {
        std::string session_name = std::string(new_object_name_);
        
        // Generate default name if empty
        if (session_name.empty()) {
            session_name = GenerateDefaultSessionName();
            LogMessage("[SESSION] Using auto-generated name: " + session_name);
        }
        
        if (session_manager_->StartNewSession(session_name)) {
            LogMessage("[SESSION] New session started: " + session_name);
            std::memset(new_object_name_, 0, sizeof(new_object_name_));
        }
    }
    
    ImGui::Columns(1);
}

bool SessionWidget::HasActiveSession() const {
    return session_manager_ && session_manager_->HasActiveSession();
}

CaptureSession* SessionWidget::GetCurrentSession() const {
    return session_manager_ ? session_manager_->GetCurrentSession() : nullptr;
}

void SessionWidget::CheckSessionStateChanged() {
    bool current_active = HasActiveSession();
    if (current_active != was_session_active_) {
        was_session_active_ = current_active;
        if (on_session_changed_) {
            on_session_changed_(current_active);
        }
    }
}

void SessionWidget::LogMessage(const std::string& message) {
    if (log_callback_) {
        log_callback_(message);
    }
}

std::string SessionWidget::GenerateDefaultSessionName() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "capture_" << std::put_time(std::localtime(&time_t), "%m%d_%H%M");
    return ss.str();
}

} // namespace SaperaCapturePro