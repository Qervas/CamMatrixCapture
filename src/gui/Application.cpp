#include "Application.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

namespace SaperaCapturePro {

Application::Application() = default;

Application::~Application() {
  Shutdown();
}

bool Application::Initialize() {
  try {
    InitializeSettings();
    InitializeGUI();
    InitializeBluetooth();
    
    AddGlobalLog("Application initialized successfully", LogLevel::kSuccess);
    return true;
  } catch (const std::exception& e) {
    std::cerr << "Failed to initialize application: " << e.what() << std::endl;
    return false;
  }
}

std::string GetExecutableDirectory() {
  #ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string exe_path(path);
    size_t pos = exe_path.find_last_of("\\/");
    return (pos != std::string::npos) ? exe_path.substr(0, pos) : ".";
  #else
    // For Linux/Mac if needed later
    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    if (count != -1) {
        path[count] = '\0';
        std::string exe_path(path);
        size_t pos = exe_path.find_last_of("/");
        return (pos != std::string::npos) ? exe_path.substr(0, pos) : ".";
    }
    return ".";
  #endif
}

void Application::InitializeSettings() {
  // Get the directory where the executable is located
  std::string exe_dir = GetExecutableDirectory();
  std::filesystem::path settings_path = std::filesystem::path(exe_dir) / "settings.json";
  std::string settings_path_str = settings_path.string();
  
  std::cout << "[SETTINGS] Executable directory: " << exe_dir << std::endl;
  std::cout << "[SETTINGS] Settings file path: " << settings_path_str << std::endl;
  
  // Check if settings file exists
  if (std::filesystem::exists(settings_path_str)) {
    std::cout << "[SETTINGS] Settings file found, loading..." << std::endl;
  } else {
    std::cout << "[SETTINGS] Settings file not found, will create with defaults" << std::endl;
  }
  
  // Initialize settings manager with absolute path to exe directory
  settings_manager_ = std::make_unique<SettingsManager>(settings_path_str);
  settings_manager_->Load();
  
  // Initialize session manager with absolute path using settings
  std::string dataset_folder = settings_manager_->GetAppSettings().last_output_folder;
  std::filesystem::path dataset_path = std::filesystem::path(exe_dir) / dataset_folder;
  std::string dataset_path_str = dataset_path.string();
  std::cout << "[DEBUG] SessionManager dataset_path: " << dataset_path_str << std::endl;
  session_manager_ = std::make_unique<SessionManager>(dataset_path_str);
}

void Application::InitializeGUI() {
  // Initialize GUI manager
  gui_manager_ = std::make_unique<GuiManager>();
  
  const auto& app_settings = settings_manager_->GetAppSettings();
  if (!gui_manager_->Initialize("Camera Matrix Capture", 
                                app_settings.window_width, 
                                app_settings.window_height,
                                app_settings.window_x,
                                app_settings.window_y)) {
    throw std::runtime_error("Failed to initialize GUI manager");
  }
  
  // Apply saved UI scale
  gui_manager_->SetUIScale(app_settings.ui_scale);
  
  // Apply VSync setting
  gui_manager_->SetVSyncEnabled(app_settings.vsync);
  
  // Initialize GUI components
  preferences_dialog_ = std::make_unique<PreferencesDialog>();
  preferences_dialog_->SetSettings(settings_manager_.get());
  preferences_dialog_->SetOnUIScaleChanged([this](float scale) {
    OnUIScaleChanged(scale);
  });
  
  log_panel_ = std::make_unique<LogPanel>();
  SetGlobalLogPanel(log_panel_.get());
}

void Application::InitializeBluetooth() {
  // Get Bluetooth manager singleton and initialize it
  bluetooth_manager_ = &BluetoothManager::GetInstance();
  if (!bluetooth_manager_->Initialize()) {
    throw std::runtime_error("Failed to initialize Bluetooth Manager");
  }
  
  // Get Camera manager singleton
  camera_manager_ = &CameraManager::GetInstance();
  
  // Set log callback for Bluetooth manager
  bluetooth_manager_->SetLogCallback([this](const std::string& message) {
    AddGlobalLog(message, LogLevel::kInfo);
  });
  
  // Initialize Bluetooth GUI with the manager
  bluetooth_gui_ = std::make_unique<BluetoothGui>();
  bluetooth_gui_->Initialize(bluetooth_manager_);
  
  AddGlobalLog("Bluetooth system initialized successfully", LogLevel::kSuccess);
}

void Application::Run() {
  while (!gui_manager_->ShouldClose() && is_running_) {
    gui_manager_->BeginFrame();
    
    // Handle keyboard shortcuts
    if (ImGui::IsKeyPressed(ImGuiKey_Comma) && ImGui::GetIO().KeyCtrl) {
      show_preferences_ = true;  // Ctrl+, opens preferences
    }
    
    // Render dockspace (includes main menu)
    RenderDockSpace();
    
    // Render panels
    if (show_camera_panel_) RenderCameraPanel();
    if (show_capture_panel_) RenderCapturePanel();
    if (show_log_panel_) log_panel_->Render(&show_log_panel_);
    if (show_bluetooth_panel_) bluetooth_gui_->Render(&show_bluetooth_panel_);
    if (show_session_manager_) RenderSessionManagerPanel();
    if (show_network_panel_) RenderNetworkPanel();
    
    // Render dialogs
    preferences_dialog_->Show(&show_preferences_);
    
    gui_manager_->EndFrame();
  }
  
  // If we exit the loop because window was closed, ensure proper shutdown
  if (gui_manager_->ShouldClose()) {
    is_running_ = false;
    // Save settings immediately when window is closed
    SaveSettings();
  }
}

void Application::Shutdown() {
  // Auto-save settings before shutdown
  SaveSettings();
  
  // Cleanup Bluetooth first (before GUI that uses it)
  if (bluetooth_gui_) {
    bluetooth_gui_->Shutdown();
    bluetooth_gui_.reset();
  }
  
  // Explicitly shutdown Bluetooth manager singleton
  if (bluetooth_manager_) {
    bluetooth_manager_->Shutdown();
    bluetooth_manager_ = nullptr;
  }
  
  // Cleanup other components
  log_panel_.reset();
  preferences_dialog_.reset();
  gui_manager_.reset();
  session_manager_.reset();
  settings_manager_.reset();
}


void Application::RenderDockSpace() {
  static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
  
  ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);
  
  window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
  window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
  
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  
  ImGui::Begin("DockSpace Demo", nullptr, window_flags);
  ImGui::PopStyleVar(3);
  
  // Render menu bar inside the dockspace window
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New Session", "Ctrl+N")) {
        session_manager_->StartNewSession("NewObject");
      }
      if (ImGui::MenuItem("Open Session...", "Ctrl+O")) {
        // TODO: Implement open session dialog
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Preferences...", "Ctrl+,")) {
        show_preferences_ = true;
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Exit", "Alt+F4")) {
        is_running_ = false;
      }
      ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("View")) {
      ImGui::MenuItem("Camera Panel", nullptr, &show_camera_panel_);
      ImGui::MenuItem("Capture Panel", nullptr, &show_capture_panel_);
      ImGui::MenuItem("Log", nullptr, &show_log_panel_);
      ImGui::MenuItem("Bluetooth", nullptr, &show_bluetooth_panel_);
      ImGui::MenuItem("Session Manager", nullptr, &show_session_manager_);
      ImGui::MenuItem("Network Settings", nullptr, &show_network_panel_);
      ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("Camera")) {
      if (ImGui::MenuItem("Discover Cameras", "F5")) {
        camera_manager_->DiscoverCameras([this](const std::string& msg) {
          AddGlobalLog(msg);
        });
      }
      if (ImGui::MenuItem("Connect All", "F6")) {
        camera_manager_->ConnectAllCameras([this](const std::string& msg) {
          AddGlobalLog(msg);
        });
      }
      if (ImGui::MenuItem("Disconnect All", "F7")) {
        camera_manager_->DisconnectAllCameras();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Capture All", "Space", false, !camera_manager_->IsCapturing())) {
        if (session_manager_->HasActiveSession() && camera_manager_->GetConnectedCount() > 0) {
          auto* session = session_manager_->GetCurrentSession();
          std::string sessionPath = session->GetNextCapturePath();
          
          // Use async capture
          camera_manager_->CaptureAllCamerasAsync(sessionPath, true, 750, [this, session, sessionPath](const std::string& msg) {
            AddGlobalLog(msg);
            if (msg.find("completed successfully") != std::string::npos) {
              session_manager_->RecordCapture(sessionPath);
            }
          });
        } else if (!session_manager_->HasActiveSession()) {
          AddGlobalLog("[SESSION] No active session - please start a session first", LogLevel::kWarning);
        } else if (camera_manager_->GetConnectedCount() == 0) {
          AddGlobalLog("[NET] No cameras connected", LogLevel::kWarning);
        }
      }
      ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem("About...")) {
        // TODO: Show about dialog
      }
      if (ImGui::MenuItem("Documentation")) {
        // TODO: Open documentation
      }
      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
  }
  
  ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
  ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
  
  // Set up default layout on first run
  static bool first_time = true;
  if (first_time) {
    first_time = false;
    
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);
    
    // Split the dockspace into sections
    auto dock_id_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f, nullptr, &dockspace_id);
    auto dock_id_right = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.4f, nullptr, &dockspace_id);
    auto dock_id_bottom = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.35f, nullptr, &dockspace_id);
    
    // Dock windows to the correct names (matching the window titles)
    ImGui::DockBuilderDockWindow("● Camera System", dock_id_left);
    ImGui::DockBuilderDockWindow("▶ Capture Control", dock_id_right);
    ImGui::DockBuilderDockWindow("Log", dock_id_bottom);
    ImGui::DockBuilderDockWindow("Bluetooth Control", dock_id_right);
    ImGui::DockBuilderDockWindow("Session Manager", dockspace_id);
    ImGui::DockBuilderDockWindow("Network Settings", dock_id_right);
    
    ImGui::DockBuilderFinish(dockspace_id);
  }
  
  ImGui::End();
}

void Application::RenderCameraPanel() {
  if (ImGui::Begin("● Camera System", &show_camera_panel_, ImGuiWindowFlags_NoCollapse)) {
    
    ImGui::Text("DIRECT Sapera SDK Camera Control");
    ImGui::Separator();
    
    // Control buttons
    if (camera_manager_->IsDiscovering()) {
      // Show buffering animation during discovery
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
      
      // Animated dots for discovery
      static float discovery_animation_time = 0.0f;
      discovery_animation_time += ImGui::GetIO().DeltaTime;
      int dot_count = static_cast<int>(discovery_animation_time * 2.0f) % 4;
      std::string button_text = "● Discovering";
      for (int i = 0; i < dot_count; i++) {
        button_text += ".";
      }
      
      ImGui::Button(button_text.c_str(), ImVec2(150, 35));
      ImGui::PopStyleColor(3);
      
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "⏳ Please wait...");
    } else {
      if (ImGui::Button("● Discover Cameras", ImVec2(150, 35))) {
        camera_manager_->DiscoverCameras([this](const std::string& msg) {
          AddGlobalLog(msg);
        });
      }
    }
    
    ImGui::SameLine();
    // Dynamic Connect/Disconnect button based on connection state
    bool has_connected = camera_manager_->GetConnectedCount() > 0;
    if (has_connected) {
      // Show Disconnect All when cameras are connected
      if (ImGui::Button("● Disconnect All", ImVec2(150, 35))) {
        camera_manager_->DisconnectAllCameras();
      }
    } else {
      // Show Connect All when no cameras are connected
      if (camera_manager_->IsConnecting()) {
        // Show buffering animation during connection
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        
        // Animated dots for connection
        static float connection_animation_time = 0.0f;
        connection_animation_time += ImGui::GetIO().DeltaTime;
        int dot_count = static_cast<int>(connection_animation_time * 2.0f) % 4;
        std::string button_text = "● Connecting";
        for (int i = 0; i < dot_count; i++) {
          button_text += ".";
        }
        
        ImGui::Button(button_text.c_str(), ImVec2(150, 35));
        ImGui::PopStyleColor(3);
        
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "⏳ Please wait...");
      } else {
        if (ImGui::Button("● Connect All", ImVec2(150, 35))) {
          camera_manager_->ConnectAllCameras([this](const std::string& msg) {
            AddGlobalLog(msg);
          });
        }
      }
    }
    
    ImGui::Separator();
    
    // Camera table
    if (ImGui::BeginTable("CameraTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Camera");
      ImGui::TableSetupColumn("Serial Number");
      ImGui::TableSetupColumn("Model");
      ImGui::TableSetupColumn("Status");
      ImGui::TableHeadersRow();
      
      const auto& cameras = camera_manager_->GetDiscoveredCameras();
      for (const auto& camera : cameras) {
        ImGui::TableNextRow();
        
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%s", camera.name.c_str());
        
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%s", camera.serialNumber.c_str());
        
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%s", camera.modelName.c_str());
        
        ImGui::TableSetColumnIndex(3);
        if (camera_manager_->IsConnected(camera.id)) {
          ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[CONN]");
        } else {
          ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[DISC]");
        }
      }
      
      ImGui::EndTable();
    }
    
    const auto& cameras = camera_manager_->GetDiscoveredCameras();
    ImGui::Text("Connected: %d / %d cameras", camera_manager_->GetConnectedCount(), static_cast<int>(cameras.size()));
    
    // Current resolution status
    if (camera_manager_->GetConnectedCount() > 0) {
      ImGui::Separator();
      const auto& params = camera_manager_->GetParameters();
      ImGui::Text("Current Resolution: %dx%d (Max: %dx%d)", 
                  params.width, params.height,
                  params.max_width, params.max_height);
      ImGui::Text("Pixel Format: %s", params.pixel_format.c_str());
    }
  }
  ImGui::End();
}

void Application::RenderCapturePanel() {
  if (ImGui::Begin("▶ Capture Control", &show_capture_panel_, ImGuiWindowFlags_NoCollapse)) {
    
    ImGui::Text("DIRECT Neural Rendering Capture");
    ImGui::Separator();
    
    // Session Management Section
    ImGui::Text("◆ Session Status");
    if (session_manager_->HasActiveSession()) {
      auto* session = session_manager_->GetCurrentSession();
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Active: %s", session->object_name.c_str());
      ImGui::Text("Captures in session: %d", session->capture_count);
      ImGui::Text("Session ID: %s", session->session_id.c_str());
      
      if (ImGui::SmallButton("■ End Session")) {
        session_manager_->EndCurrentSession();
        AddGlobalLog("[SESSION] Session ended from capture panel", LogLevel::kInfo);
      }
    } else {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No active session");
      ImGui::Text("Start a new session to begin capturing");
      
      static char new_object_name[256] = "";
      ImGui::Text("Object Name:");
      ImGui::SetNextItemWidth(200);
      ImGui::InputText("##NewObjectName", new_object_name, sizeof(new_object_name));
      ImGui::SameLine();
      if (ImGui::Button("▶ Start Session")) {
        std::string session_name = std::string(new_object_name);
        
        // Provide default name if user forgot to input
        if (session_name.empty()) {
          auto now = std::chrono::system_clock::now();
          auto time_t = std::chrono::system_clock::to_time_t(now);
          std::stringstream ss;
          ss << "capture_" << std::put_time(std::localtime(&time_t), "%m%d_%H%M");
          session_name = ss.str();
          AddGlobalLog("[SESSION] Using default session name: " + session_name, LogLevel::kInfo);
        }
        
        if (session_manager_->StartNewSession(session_name)) {
          AddGlobalLog("[SESSION] New session started: " + session_name, LogLevel::kSuccess);
          memset(new_object_name, 0, sizeof(new_object_name));
        }
      }
    }
    
    ImGui::Separator();
    
    // Dataset info
    ImGui::Text("◆ Dataset Configuration");
    const auto& app_settings = settings_manager_->GetAppSettings();
    ImGui::Text("Dataset: %s", app_settings.last_output_folder.c_str());
    
    // Format selection
    ImGui::Text("Format:");
    ImGui::SameLine();
    bool capture_format_raw = camera_manager_->GetCaptureFormat();
    if (ImGui::RadioButton("TIFF", !capture_format_raw)) {
      camera_manager_->SetCaptureFormat(false);
      AddGlobalLog("[IMG] Format set to TIFF", LogLevel::kInfo);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("RAW", capture_format_raw)) {
      camera_manager_->SetCaptureFormat(true);
      AddGlobalLog("[IMG] Format set to RAW", LogLevel::kInfo);
    }
    
    ImGui::Separator();
    
    // Capture section
    ImGui::Text("◆ Capture Control");
    bool can_capture = camera_manager_->GetConnectedCount() > 0 && session_manager_->HasActiveSession() && !camera_manager_->IsCapturing();
    
    if (can_capture) {
      if (ImGui::Button("▶ START CAPTURE", ImVec2(300, 60))) {
        auto* session = session_manager_->GetCurrentSession();
        std::string sessionPath = session->GetNextCapturePath();
        
        // Use async capture to prevent GUI freezing
        camera_manager_->CaptureAllCamerasAsync(sessionPath, true, 750, [this, session, sessionPath](const std::string& msg) {
          AddGlobalLog(msg);
          if (msg.find("completed successfully") != std::string::npos) {
            session_manager_->RecordCapture(sessionPath);
          }
        });
      }
    } else if (camera_manager_->IsCapturing()) {
      // Show progress indicator when capturing
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.0f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.6f, 0.0f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.6f, 0.0f, 1.0f));
      
      // Add spinning animation
      static float rotation = 0.0f;
      rotation += ImGui::GetIO().DeltaTime * 3.0f;
      if (rotation > 6.28f) rotation = 0.0f;
      
      ImGui::Button("⏳ CAPTURING...", ImVec2(300, 60));
      ImGui::PopStyleColor(3);
    } else {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
      ImGui::Button("▶ START CAPTURE", ImVec2(300, 60));
      ImGui::PopStyleVar();
      
      if (!session_manager_->HasActiveSession()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Need active session to capture");
      }
      if (camera_manager_->GetConnectedCount() == 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Need connected cameras to capture");
      }
    }
    
    // Status display
    ImGui::Separator();
    ImGui::Text("◆ System Status");
    const auto& cameras = camera_manager_->GetDiscoveredCameras();
    int connected_count = camera_manager_->GetConnectedCount();
    int total_count = static_cast<int>(cameras.size());
    ImGui::Text("Cameras: %d/%d connected", connected_count, total_count);
    
    if (camera_manager_->IsCapturing()) {
      ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "[BUSY] Capturing images...");
    } else if (connected_count > 0 && session_manager_->HasActiveSession()) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[OK] Ready to capture");
    } else if (connected_count == 0) {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[WARN] No cameras connected");
    } else {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[WARN] No active session");
    }
    
    // Exposure control
    if (camera_manager_->GetConnectedCount() > 0) {
      ImGui::Separator();
      ImGui::Text("◆ Basic Camera Settings");
      
      int exposure_time = camera_manager_->GetExposureTime();
      if (ImGui::SliderInt("Exposure Time (µs)", &exposure_time, 100, 100000)) {
        camera_manager_->SetExposureTime(exposure_time);
        camera_manager_->ApplyParameterToAllCameras("ExposureTime", std::to_string(exposure_time));
        AddGlobalLog("[PARAM] Exposure time set to " + std::to_string(exposure_time) + "µs", LogLevel::kInfo);
      }
    }
    
    // File Explorer Section - Show captured images if there's an active session
    if (session_manager_->HasActiveSession()) {
      ImGui::Separator();
      ImGui::Text("◆ Captured Images");
      
      auto* session = session_manager_->GetCurrentSession();
      const auto& capture_paths = session->capture_paths;
      
      if (capture_paths.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No captures yet - take your first capture!");
      } else {
        // Create a scrollable region for the file list
        ImGui::BeginChild("file_explorer", ImVec2(0, 200), true);
        
        for (size_t i = 0; i < capture_paths.size(); i++) {
          const std::string& path = capture_paths[i];
          std::filesystem::path fs_path(path);
          
          // Check if directory exists and list image files
          if (std::filesystem::exists(fs_path) && std::filesystem::is_directory(fs_path)) {
            if (ImGui::TreeNode(("Capture " + std::to_string(i + 1) + " - " + fs_path.filename().string()).c_str())) {
              
              // List all image files in this capture directory
              try {
                for (const auto& entry : std::filesystem::directory_iterator(fs_path)) {
                  if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    
                    if (ext == ".tiff" || ext == ".tif" || ext == ".raw" || ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
                      bool is_selected = (selected_image_path_ == entry.path().string());
                      if (ImGui::Selectable(entry.path().filename().string().c_str(), is_selected)) {
                        selected_image_path_ = entry.path().string();
                        AddGlobalLog("[FILE] Selected: " + selected_image_path_, LogLevel::kInfo);
                      }
                      
                      // Show file info on hover
                      if (ImGui::IsItemHovered()) {
                        if (ImGui::BeginTooltip()) {
                          ImGui::Text("File: %s", entry.path().filename().string().c_str());
                          ImGui::Text("Path: %s", entry.path().string().c_str());
                          
                          // Try to get file size
                          std::error_code ec;
                          auto file_size = std::filesystem::file_size(entry.path(), ec);
                          if (!ec) {
                            if (file_size > 1024 * 1024) {
                              ImGui::Text("Size: %.2f MB", file_size / (1024.0f * 1024.0f));
                            } else if (file_size > 1024) {
                              ImGui::Text("Size: %.2f KB", file_size / 1024.0f);
                            } else {
                              ImGui::Text("Size: %llu bytes", file_size);
                            }
                          }
                          
                          ImGui::EndTooltip();
                        }
                      }
                      
                      // Double-click to open photo directly
                      if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                        std::string command = "start \"\" \"" + entry.path().string() + "\"";
                        system(command.c_str());
                        AddGlobalLog("[FILE] Opening photo: " + entry.path().string(), LogLevel::kInfo);
                      }
                    }
                  }
                }
              } catch (const std::exception&) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Error reading directory");
              }
              
              ImGui::TreePop();
            }
          } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Capture %zu - Directory not found", i + 1);
          }
        }
        
        ImGui::EndChild();
        
        // Quick action buttons
        ImGui::Spacing();
        if (ImGui::Button("📁 Open Session Folder", ImVec2(180, 0))) {
          std::string folder_path = session->base_path;
          std::cout << "[DEBUG] Session base_path: " << folder_path << std::endl;
          
          // Convert to absolute path if needed
          std::filesystem::path abs_path = std::filesystem::absolute(folder_path);
          std::string abs_folder_path = abs_path.string();
          std::cout << "[DEBUG] Absolute path: " << abs_folder_path << std::endl;
          
          std::string command = "explorer \"" + abs_folder_path + "\"";
          std::cout << "[DEBUG] Explorer command: " << command << std::endl;
          system(command.c_str());
          AddGlobalLog("[FILE] Opening session folder: " + abs_folder_path, LogLevel::kInfo);
        }
        
        ImGui::SameLine();
        if (ImGui::Button("🔄 Refresh", ImVec2(80, 0))) {
          AddGlobalLog("[FILE] File list refreshed", LogLevel::kInfo);
        }
        
        if (!selected_image_path_.empty()) {
          ImGui::SameLine();
          if (ImGui::Button("❌ Clear Selection", ImVec2(120, 0))) {
            selected_image_path_.clear();
            AddGlobalLog("[FILE] Selection cleared", LogLevel::kInfo);
          }
        }
        
        // Image Preview Section
        if (!selected_image_path_.empty()) {
          ImGui::Separator();
          ImGui::Text("◆ Image Preview");
          
          std::filesystem::path selected_path(selected_image_path_);
          if (std::filesystem::exists(selected_path)) {
            ImGui::Text("Selected: %s", selected_path.filename().string().c_str());
            
            // Show image info
            std::error_code ec;
            auto file_size = std::filesystem::file_size(selected_path, ec);
            if (!ec) {
              if (file_size > 1024 * 1024) {
                ImGui::Text("Size: %.2f MB", file_size / (1024.0f * 1024.0f));
              } else {
                ImGui::Text("Size: %.2f KB", file_size / 1024.0f);
              }
            }
            
            // Quick action buttons for the selected image
            if (ImGui::Button("🔍 View in Explorer", ImVec2(150, 0))) {
              std::cout << "[DEBUG] Selected image path: " << selected_image_path_ << std::endl;
              
              // Convert to absolute path if needed
              std::filesystem::path abs_path = std::filesystem::absolute(selected_image_path_);
              std::string abs_image_path = abs_path.string();
              std::cout << "[DEBUG] Absolute image path: " << abs_image_path << std::endl;
              
              std::string command = "explorer /select,\"" + abs_image_path + "\"";
              std::cout << "[DEBUG] Explorer select command: " << command << std::endl;
              system(command.c_str());
              AddGlobalLog("[FILE] Opening image location: " + abs_image_path, LogLevel::kInfo);
            }
            
            ImGui::SameLine();
            if (ImGui::Button("📱 Open with Default App", ImVec2(180, 0))) {
              std::string command = "start \"\" \"" + selected_image_path_ + "\"";
              system(command.c_str());
              AddGlobalLog("[FILE] Opening image with default app: " + selected_image_path_, LogLevel::kInfo);
            }
            
            // Placeholder for actual image preview (would need image loading library)
            ImGui::Spacing();
            ImGui::BeginChild("image_preview_area", ImVec2(0, 150), true);
            ImGui::Text("📷 Image Preview");
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "File: %s", selected_path.filename().string().c_str());
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Preview would appear here");
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Click buttons above to view the image");
            ImGui::EndChild();
            
          } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Selected file no longer exists");
            if (ImGui::Button("Clear Selection")) {
              selected_image_path_.clear();
            }
          }
        }
        
        // Show session info
        ImGui::Separator();
        ImGui::Text("Session: %s", session->object_name.c_str());
        ImGui::Text("Total Captures: %d", static_cast<int>(capture_paths.size()));
        ImGui::Text("Session Path: %s", session->base_path.c_str());
      }
    }
  }
  ImGui::End();
}

void Application::RenderSessionManagerPanel() {
  if (ImGui::Begin("Session Manager", &show_session_manager_)) {
    // TODO: Implement session manager content
    ImGui::Text("Session management will be here");
  }
  ImGui::End();
}


void Application::RenderNetworkPanel() {
  if (ImGui::Begin("Network Settings", &show_network_panel_)) {
    // TODO: Implement network settings content
    ImGui::Text("Network settings will be here");
  }
  ImGui::End();
}

void Application::OnUIScaleChanged(float scale) {
  gui_manager_->SetUIScale(scale);
  
  // Auto-save UI scale to local cache
  settings_manager_->GetAppSettings().ui_scale = scale;
  settings_manager_->Save();
  
  AddGlobalLog("UI scale changed to " + std::to_string(scale) + " and saved", LogLevel::kInfo);
}

void Application::SaveSettings() {
  if (settings_manager_) {
    // Save window size and position
    int width, height;
    glfwGetWindowSize(gui_manager_->GetWindow(), &width, &height);
    settings_manager_->GetAppSettings().window_width = width;
    settings_manager_->GetAppSettings().window_height = height;
    
    // Also save window position
    int xpos, ypos;
    glfwGetWindowPos(gui_manager_->GetWindow(), &xpos, &ypos);
    settings_manager_->GetAppSettings().window_x = xpos;
    settings_manager_->GetAppSettings().window_y = ypos;
    
    settings_manager_->Save();
  }
}

void Application::LoadSettings() {
  if (settings_manager_) {
    settings_manager_->Load();
  }
}

}  // namespace SaperaCapturePro