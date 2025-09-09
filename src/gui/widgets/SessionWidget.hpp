#pragma once

#include <memory>
#include <string>
#include <functional>
#include <imgui.h>
#include "../../utils/SessionManager.hpp"

namespace SaperaCapturePro {

class SessionWidget {
public:
    SessionWidget();
    ~SessionWidget();

    void Initialize(SessionManager* session_manager);
    void Render();
    void Shutdown();

    // State queries
    bool HasActiveSession() const;
    CaptureSession* GetCurrentSession() const;
    
    // Callbacks
    void SetOnSessionChanged(std::function<void(bool has_session)> callback) {
        on_session_changed_ = callback;
    }
    
    void SetLogCallback(std::function<void(const std::string&)> callback) {
        log_callback_ = callback;
    }

private:
    // System references
    SessionManager* session_manager_ = nullptr;
    
    // UI state
    char new_object_name_[256] = "";
    bool was_session_active_ = false; // To detect session state changes
    
    // Callbacks
    std::function<void(bool has_session)> on_session_changed_;
    std::function<void(const std::string&)> log_callback_;
    
    // Helper methods
    void RenderActiveSession();
    void RenderSessionCreator();
    void LogMessage(const std::string& message);
    void CheckSessionStateChanged();
    std::string GenerateDefaultSessionName() const;
};

} // namespace SaperaCapturePro