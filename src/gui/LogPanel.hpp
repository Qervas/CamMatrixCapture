#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <mutex>
#include <imgui.h>

namespace SaperaCapturePro {

enum class LogLevel {
  kInfo,
  kWarning,
  kError,
  kDebug,
  kSuccess
};

struct LogMessage {
  std::string message;
  LogLevel level;
  std::chrono::system_clock::time_point timestamp;
};

class LogPanel {
 public:
  LogPanel();
  ~LogPanel() = default;

  void Render(bool* p_open);
  
  void AddLog(const std::string& message, LogLevel level = LogLevel::kInfo);
  void Clear();
  
  void SetAutoScroll(bool enabled) { auto_scroll_ = enabled; }
  bool GetAutoScroll() const { return auto_scroll_; }
  
  void SetFilterNetworkLogs(bool filter) { filter_network_logs_ = filter; }
  bool GetFilterNetworkLogs() const { return filter_network_logs_; }

 private:
  std::vector<LogMessage> messages_;
  std::mutex messages_mutex_;
  
  bool auto_scroll_ = true;
  bool filter_network_logs_ = true;
  char filter_buffer_[256] = "";
  
  // Log level filters
  bool show_info_ = true;
  bool show_warnings_ = true;
  bool show_errors_ = true;
  bool show_debug_ = false;
  bool show_success_ = true;
  
  void RenderToolbar();
  void RenderMessages();
  bool ShouldShowMessage(const LogMessage& msg) const;
  ImVec4 GetLogColor(LogLevel level) const;
  const char* GetLogPrefix(LogLevel level) const;
};

// Global log instance accessor
LogPanel* GetGlobalLogPanel();
void SetGlobalLogPanel(LogPanel* panel);

// Convenience function for global logging
void AddGlobalLog(const std::string& message, LogLevel level = LogLevel::kInfo);

}  // namespace SaperaCapturePro