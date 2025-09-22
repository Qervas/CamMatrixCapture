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
    ImGui::BeginChild("SessionWidget", ImVec2(0, 45), true, ImGuiWindowFlags_NoScrollbar);
    
    ImGui::Text("📁 Session:");
    ImGui::SameLine();
    
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

    // Compact horizontal layout - all in one line
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ %s", session->object_name.c_str());
    ImGui::SameLine();
    ImGui::Text("[%s]", session->session_id.substr(0, 8).c_str());
    ImGui::SameLine();
    ImGui::Text("| 📸 %d", session->capture_count);
    ImGui::SameLine();
    ImGui::Text("| 📂 %zu", session->capture_paths.size());
    ImGui::SameLine();

    // Right-aligned buttons
    float button_width = 80.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float pos = ImGui::GetWindowWidth() - button_width * 2 - spacing * 2 - ImGui::GetStyle().WindowPadding.x;
    if (pos > ImGui::GetCursorPosX())
        ImGui::SetCursorPosX(pos);

    if (ImGui::Button("📂 Open", ImVec2(button_width, 0))) {
        std::filesystem::path abs_path = std::filesystem::absolute(session->base_path);
        std::string command = "explorer \"" + abs_path.string() + "\"";
        system(command.c_str());
        LogMessage("[SESSION] Opened session folder: " + abs_path.string());
    }
    ImGui::SameLine();
    if (ImGui::Button("⏹ End", ImVec2(button_width, 0))) {
        session_manager_->EndCurrentSession();
        LogMessage("[SESSION] Session ended from session widget");
    }
}

void SessionWidget::RenderSessionCreator() {
    // Compact horizontal layout - all in one line
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "⚠ No session");
    ImGui::SameLine();

    ImGui::Text("Name:");
    ImGui::SameLine();

    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##NewObjectName", "auto-generate if empty",
                           new_object_name_, sizeof(new_object_name_));
    ImGui::SameLine();

    if (ImGui::Button("▶ Start Session", ImVec2(120, 0))) {
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