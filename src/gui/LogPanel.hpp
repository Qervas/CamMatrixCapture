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
  void RenderContent();  // Render without window wrapper

  void AddLog(const std::string& message, LogLevel level = LogLevel::kInfo);
  void Clear();
  void SaveToFile(const std::string& filename);

  void SetAutoScroll(bool enabled) { auto_scroll_ = enabled; }
  bool GetAutoScroll() const { return auto_scroll_; }

  void SetFilterNetworkLogs(bool filter) { filter_network_logs_ = filter; }
  bool GetFilterNetworkLogs() const { return filter_network_logs_; }

  void SetMaxMessages(size_t max) { max_messages_ = max; }
  size_t GetMaxMessages() const { return max_messages_; }

  void SetAutoDeleteEnabled(bool enabled) { auto_delete_enabled_ = enabled; }
  bool GetAutoDeleteEnabled() const { return auto_delete_enabled_; }

 private:
  std::vector<LogMessage> messages_;
  std::mutex messages_mutex_;

  bool auto_scroll_ = true;
  bool filter_network_logs_ = true;
  char filter_buffer_[256] = "";

  // Log history settings
  size_t max_messages_ = 0;  // 0 = unlimited
  bool auto_delete_enabled_ = false;  // Default: no auto-deletion

  // Log level filters
  bool show_info_ = true;
  bool show_warnings_ = true;
  bool show_errors_ = true;
  bool show_debug_ = false;
  bool show_success_ = true;

  // Save dialog state
  bool show_save_dialog_ = false;
  char save_filename_[256] = "log_export.txt";
  
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