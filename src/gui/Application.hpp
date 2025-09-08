#pragma once

#include <memory>
#include <vector>
#include <string>
#include "GuiManager.hpp"
#include "PreferencesDialog.hpp"
#include "LogPanel.hpp"
#include "BluetoothGui.hpp"
#include "../utils/SettingsManager.hpp"
#include "../utils/SessionManager.hpp"
#include "../bluetooth/BluetoothManager.hpp"

namespace SaperaCapturePro {

class Application {
 public:
  Application();
  ~Application();

  bool Initialize();
  void Run();
  void Shutdown();

 private:
  // Core managers
  std::unique_ptr<GuiManager> gui_manager_;
  std::unique_ptr<SettingsManager> settings_manager_;
  std::unique_ptr<SessionManager> session_manager_;
  BluetoothManager* bluetooth_manager_ = nullptr;  // Singleton
  
  // GUI components
  std::unique_ptr<PreferencesDialog> preferences_dialog_;
  std::unique_ptr<LogPanel> log_panel_;
  std::unique_ptr<BluetoothGui> bluetooth_gui_;
  
  // Window states
  bool show_preferences_ = false;
  bool show_camera_panel_ = true;
  bool show_capture_panel_ = true;
  bool show_log_panel_ = true;
  bool show_bluetooth_panel_ = false;
  bool show_session_manager_ = false;
  bool show_color_panel_ = true;
  bool show_network_panel_ = false;
  
  // Application state
  bool is_running_ = true;
  
  void InitializeSettings();
  void InitializeGUI();
  void InitializeBluetooth();
  
  void RenderMainMenuBar();
  void RenderDockSpace();
  void RenderCameraPanel();
  void RenderCapturePanel();
  void RenderSessionManagerPanel();
  void RenderColorPanel();
  void RenderNetworkPanel();
  
  void OnUIScaleChanged(float scale);
  void SaveSettings();
  void LoadSettings();
};

}  // namespace SaperaCapturePro