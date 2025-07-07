#pragma once

#include <vector>
#include <string>

class StatusPanel {
public:
    StatusPanel();
    ~StatusPanel();

    void Initialize();
    void Render(const std::vector<std::pair<std::string, std::string>>& log_messages);

    bool visible = true;

private:
    // UI state
    bool auto_scroll_ = true;
    bool show_timestamps_ = true;
    char filter_text_[256] = "";
    int log_level_filter_ = 0; // 0=All, 1=Info+, 2=Warning+, 3=Error only
    
    void RenderLogControls();
    void RenderLogTable(const std::vector<std::pair<std::string, std::string>>& messages);
    void RenderSystemMonitoring();
    bool ShouldShowMessage(const std::string& message, const std::string& level);
    ImVec4 GetLogLevelColor(const std::string& level);
}; 