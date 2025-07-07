#include "StatusPanel.hpp"
#include <imgui.h>
#include <cstring>
#include <algorithm>

StatusPanel::StatusPanel() {
    std::memset(filter_text_, 0, sizeof(filter_text_));
}

StatusPanel::~StatusPanel() = default;

void StatusPanel::Initialize() {
    // Initialize any resources if needed
}

void StatusPanel::Render(const std::vector<std::pair<std::string, std::string>>& log_messages) {
    if (!visible) return;

    ImGui::Begin("System Status & Logs", &visible);

    // System monitoring section
    RenderSystemMonitoring();
    
    ImGui::Separator();
    
    // Log controls
    RenderLogControls();
    
    ImGui::Separator();
    
    // Log table
    RenderLogTable(log_messages);

    ImGui::End();
}

void StatusPanel::RenderSystemMonitoring() {
    ImGui::Text("System Monitoring");
    
    // Performance metrics
    ImGui::Text("Application Performance:");
    ImGui::Indent();
    ImGui::Text("Frame Rate: %.1f FPS", ImGui::GetIO().Framerate);
    ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
    ImGui::Unindent();
    
    // Memory usage (simplified)
    ImGui::Text("Memory Usage:");
    ImGui::Indent();
    ImGui::Text("ImGui Vertices: %d", ImGui::GetIO().MetricsRenderVertices);
    ImGui::Text("ImGui Indices: %d", ImGui::GetIO().MetricsRenderIndices);
    ImGui::Text("ImGui Windows: %d", ImGui::GetIO().MetricsRenderWindows);
    ImGui::Unindent();
    
    // System status indicators
    ImGui::Text("System Status:");
    ImGui::Indent();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ GUI System: Online");
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Event System: Active");
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "⚠ Camera System: Pending");
    ImGui::Unindent();
}

void StatusPanel::RenderLogControls() {
    ImGui::Text("Log Controls");
    
    // Filter text
    ImGui::Text("Filter:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputText("##LogFilter", filter_text_, sizeof(filter_text_));
    
    ImGui::SameLine();
    if (ImGui::Button("Clear Filter")) {
        std::memset(filter_text_, 0, sizeof(filter_text_));
    }
    
    // Log level filter
    ImGui::SameLine();
    ImGui::Text("Level:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    const char* levels[] = { "All", "Info+", "Warning+", "Error Only" };
    ImGui::Combo("##LogLevel", &log_level_filter_, levels, 4);
    
    // Display options
    ImGui::Checkbox("Auto-scroll", &auto_scroll_);
    ImGui::SameLine();
    ImGui::Checkbox("Show timestamps", &show_timestamps_);
    
    // Log actions
    ImGui::SameLine();
    if (ImGui::Button("Clear Logs")) {
        // This would need to be implemented in the main GUI class
        ImGui::SetTooltip("Clear logs functionality not implemented yet");
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Export Logs")) {
        // This would need to be implemented
        ImGui::SetTooltip("Export logs functionality not implemented yet");
    }
}

void StatusPanel::RenderLogTable(const std::vector<std::pair<std::string, std::string>>& messages) {
    // Filter messages
    std::vector<std::pair<std::string, std::string>> filtered_messages;
    
    for (const auto& msg_pair : messages) {
        if (ShouldShowMessage(msg_pair.first, msg_pair.second)) {
            filtered_messages.push_back(msg_pair);
        }
    }
    
    // Log table
    if (ImGui::BeginTable("LogTable", show_timestamps_ ? 3 : 2, 
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | 
                          ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                          ImVec2(0, 300))) {
        
        // Table headers
        if (show_timestamps_) {
            ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 80);
        }
        ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        
        // Table rows
        for (const auto& msg_pair : filtered_messages) {
            const std::string& full_message = msg_pair.first;
            const std::string& level = msg_pair.second;
            
            ImGui::TableNextRow();
            
            // Extract timestamp and message from full_message
            std::string timestamp, message;
            size_t bracket_end = full_message.find(']');
            if (bracket_end != std::string::npos && full_message.length() > 0 && full_message[0] == '[') {
                timestamp = full_message.substr(1, bracket_end - 1);
                
                // Find level separator
                size_t level_start = full_message.find(level + ":");
                if (level_start != std::string::npos) {
                    message = full_message.substr(level_start + level.length() + 2); // +2 for ": "
                } else {
                    message = full_message.substr(bracket_end + 2);
                }
            } else {
                timestamp = "N/A";
                message = full_message;
            }
            
            // Timestamp column
            if (show_timestamps_) {
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", timestamp.c_str());
            }
            
            // Level column
            ImGui::TableSetColumnIndex(show_timestamps_ ? 1 : 0);
            ImVec4 level_color = GetLogLevelColor(level);
            ImGui::TextColored(level_color, "%s", level.c_str());
            
            // Message column
            ImGui::TableSetColumnIndex(show_timestamps_ ? 2 : 1);
            ImGui::TextWrapped("%s", message.c_str());
        }
        
        // Auto-scroll to bottom
        if (auto_scroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
        
        ImGui::EndTable();
    }
    
    // Log statistics
    ImGui::Text("Showing %d of %d messages", 
                static_cast<int>(filtered_messages.size()), 
                static_cast<int>(messages.size()));
}

bool StatusPanel::ShouldShowMessage(const std::string& message, const std::string& level) {
    // Apply level filter
    if (log_level_filter_ == 1 && level == "DEBUG") return false;
    if (log_level_filter_ == 2 && (level == "DEBUG" || level == "INFO")) return false;
    if (log_level_filter_ == 3 && level != "ERROR") return false;
    
    // Apply text filter
    if (strlen(filter_text_) > 0) {
        std::string msg_lower = message;
        std::string filter_lower = filter_text_;
        std::transform(msg_lower.begin(), msg_lower.end(), msg_lower.begin(), ::tolower);
        std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(), ::tolower);
        
        if (msg_lower.find(filter_lower) == std::string::npos) {
            return false;
        }
    }
    
    return true;
}

ImVec4 StatusPanel::GetLogLevelColor(const std::string& level) {
    if (level == "ERROR") {
        return ImVec4(1.0f, 0.3f, 0.3f, 1.0f); // Red
    } else if (level == "WARNING" || level == "WARN") {
        return ImVec4(1.0f, 0.8f, 0.0f, 1.0f); // Yellow
    } else if (level == "INFO") {
        return ImVec4(0.3f, 0.8f, 1.0f, 1.0f); // Blue
    } else if (level == "DEBUG") {
        return ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Gray
    } else {
        return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White
    }
} 