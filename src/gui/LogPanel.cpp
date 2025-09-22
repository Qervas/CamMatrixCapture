#include "LogPanel.hpp"
#include <imgui.h>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <fstream>

namespace SaperaCapturePro {

static LogPanel* g_log_panel = nullptr;

LogPanel::LogPanel() {
  AddLog("Log system initialized", LogLevel::kInfo);
}

void LogPanel::Render(bool* p_open) {
  if (!p_open || !*p_open) return;
  
  if (ImGui::Begin("Log", p_open)) {
    RenderToolbar();
    ImGui::Separator();
    RenderMessages();
  }
  ImGui::End();
}

void LogPanel::AddLog(const std::string& message, LogLevel level) {
  std::lock_guard<std::mutex> lock(messages_mutex_);

  LogMessage msg;
  msg.message = message;
  msg.level = level;
  msg.timestamp = std::chrono::system_clock::now();

  messages_.push_back(msg);

  // Only limit log size if auto-delete is enabled and max_messages is set
  if (auto_delete_enabled_ && max_messages_ > 0 && messages_.size() > max_messages_) {
    messages_.erase(messages_.begin(), messages_.begin() + (messages_.size() - max_messages_));
  }
}

void LogPanel::Clear() {
  std::lock_guard<std::mutex> lock(messages_mutex_);
  messages_.clear();
  AddLog("Log cleared", LogLevel::kInfo);
}

void LogPanel::RenderToolbar() {
  if (ImGui::Button("Clear")) {
    Clear();
  }

  ImGui::SameLine();
  if (ImGui::Button("Save...")) {
    show_save_dialog_ = true;
  }

  ImGui::SameLine();
  ImGui::Text("| Messages: %zu", messages_.size());
  if (auto_delete_enabled_ && max_messages_ > 0) {
    ImGui::SameLine();
    ImGui::Text("(max: %zu)", max_messages_);
  }

  ImGui::SameLine();
  ImGui::Checkbox("Auto-scroll", &auto_scroll_);
  
  ImGui::SameLine();
  ImGui::Separator();
  ImGui::SameLine();
  
  // Log level filters
  ImGui::Text("Show:");
  ImGui::SameLine();
  
  ImGui::PushStyleColor(ImGuiCol_Text, GetLogColor(LogLevel::kInfo));
  ImGui::Checkbox("Info", &show_info_);
  ImGui::PopStyleColor();
  
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, GetLogColor(LogLevel::kWarning));
  ImGui::Checkbox("Warnings", &show_warnings_);
  ImGui::PopStyleColor();
  
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, GetLogColor(LogLevel::kError));
  ImGui::Checkbox("Errors", &show_errors_);
  ImGui::PopStyleColor();
  
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, GetLogColor(LogLevel::kDebug));
  ImGui::Checkbox("Debug", &show_debug_);
  ImGui::PopStyleColor();
  
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, GetLogColor(LogLevel::kSuccess));
  ImGui::Checkbox("Success", &show_success_);
  ImGui::PopStyleColor();
  
  ImGui::SameLine();
  ImGui::Separator();
  ImGui::SameLine();
  
  ImGui::Checkbox("Filter Network", &filter_network_logs_);
  
  // Search filter
  ImGui::SameLine();
  ImGui::SetNextItemWidth(200);
  ImGui::InputText("Filter", filter_buffer_, sizeof(filter_buffer_));
}

void LogPanel::RenderMessages() {
  // Save file dialog
  if (show_save_dialog_) {
    ImGui::OpenPopup("Save Log");
    show_save_dialog_ = false;
  }

  if (ImGui::BeginPopupModal("Save Log", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Enter filename:");
    ImGui::InputText("##SaveFilename", save_filename_, sizeof(save_filename_));

    if (ImGui::Button("Save", ImVec2(120, 0))) {
      SaveToFile(save_filename_);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  ImGui::BeginChild("LogMessages", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
  
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
  
  {
    std::lock_guard<std::mutex> lock(messages_mutex_);
    
    for (const auto& msg : messages_) {
      if (!ShouldShowMessage(msg)) continue;
      
      // Format timestamp
      auto time_t = std::chrono::system_clock::to_time_t(msg.timestamp);
      std::tm tm = {};
      localtime_s(&tm, &time_t);
      
      std::stringstream timestamp;
      timestamp << std::put_time(&tm, "[%H:%M:%S] ");
      
      // Render message with color
      ImGui::PushStyleColor(ImGuiCol_Text, GetLogColor(msg.level));
      ImGui::TextUnformatted((timestamp.str() + GetLogPrefix(msg.level) + msg.message).c_str());
      ImGui::PopStyleColor();
    }
  }
  
  ImGui::PopStyleVar();
  
  if (auto_scroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
    ImGui::SetScrollHereY(1.0f);
  }
  
  ImGui::EndChild();
}

bool LogPanel::ShouldShowMessage(const LogMessage& msg) const {
  // Check log level filter
  switch (msg.level) {
    case LogLevel::kInfo: if (!show_info_) return false; break;
    case LogLevel::kWarning: if (!show_warnings_) return false; break;
    case LogLevel::kError: if (!show_errors_) return false; break;
    case LogLevel::kDebug: if (!show_debug_) return false; break;
    case LogLevel::kSuccess: if (!show_success_) return false; break;
  }
  
  // Check network filter
  if (filter_network_logs_) {
    if (msg.message.find("Network") != std::string::npos ||
        msg.message.find("Bandwidth") != std::string::npos ||
        msg.message.find("Packet") != std::string::npos) {
      return false;
    }
  }
  
  // Check text filter
  if (filter_buffer_[0] != '\0') {
    std::string lower_msg = msg.message;
    std::string lower_filter = filter_buffer_;
    std::transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(), ::tolower);
    std::transform(lower_filter.begin(), lower_filter.end(), lower_filter.begin(), ::tolower);
    
    if (lower_msg.find(lower_filter) == std::string::npos) {
      return false;
    }
  }
  
  return true;
}

void LogPanel::SaveToFile(const std::string& filename) {
  std::ofstream file(filename);
  if (!file.is_open()) {
    AddLog("Failed to save log to file: " + filename, LogLevel::kError);
    return;
  }

  std::lock_guard<std::mutex> lock(messages_mutex_);

  file << "=== Camera Matrix Capture Log Export ===\n";
  file << "Total messages: " << messages_.size() << "\n\n";

  for (const auto& msg : messages_) {
    // Format timestamp
    auto time_t = std::chrono::system_clock::to_time_t(msg.timestamp);
    std::tm tm = {};
    localtime_s(&tm, &time_t);

    file << std::put_time(&tm, "[%Y-%m-%d %H:%M:%S] ");
    file << GetLogPrefix(msg.level);
    file << msg.message;
    file << "\n";
  }

  file.close();
  AddLog("Log saved to: " + filename, LogLevel::kSuccess);
}

ImVec4 LogPanel::GetLogColor(LogLevel level) const {
  switch (level) {
    case LogLevel::kInfo: return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    case LogLevel::kWarning: return ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
    case LogLevel::kError: return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
    case LogLevel::kDebug: return ImVec4(0.5f, 0.5f, 1.0f, 1.0f);
    case LogLevel::kSuccess: return ImVec4(0.3f, 0.9f, 0.3f, 1.0f);
    default: return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
  }
}

const char* LogPanel::GetLogPrefix(LogLevel level) const {
  switch (level) {
    case LogLevel::kInfo: return "[INFO] ";
    case LogLevel::kWarning: return "[WARN] ";
    case LogLevel::kError: return "[ERROR] ";
    case LogLevel::kDebug: return "[DEBUG] ";
    case LogLevel::kSuccess: return "[OK] ";
    default: return "";
  }
}

LogPanel* GetGlobalLogPanel() {
  return g_log_panel;
}

void SetGlobalLogPanel(LogPanel* panel) {
  g_log_panel = panel;
}

void AddGlobalLog(const std::string& message, LogLevel level) {
  if (g_log_panel) {
    g_log_panel->AddLog(message, level);
  }
}

}  // namespace SaperaCapturePro