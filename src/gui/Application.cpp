#include "Application.hpp"
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h>
#include "../utils/SessionManager.hpp"
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
  
  // Initialize unified capture studio panel
  capture_studio_panel_ = std::make_unique<CaptureStudioPanel>();
  capture_studio_panel_->SetLogCallback([this](const std::string& message) {
    AddGlobalLog(message);
  });

  // Push color conversion preferences to camera manager
  if (camera_manager_) {
    const auto& cs = settings_manager_->GetCameraSettings();
    CameraManager::ColorConfig cc;
    cc.color_method = cs.color_method;
    cc.bayer_align = cs.bayer_align;
    cc.use_hardware = cs.use_hardware_conversion;
    cc.color_output_format = cs.color_output_format;
    cc.gamma = cs.gamma;
    cc.wb_gain_r = cs.white_balance_red;
    cc.wb_gain_g = cs.white_balance_green;
    cc.wb_gain_b = cs.white_balance_blue;
    cc.wb_offset_r = cs.white_balance_offset_red;
    cc.wb_offset_g = cs.white_balance_offset_green;
    cc.wb_offset_b = cs.white_balance_offset_blue;
    camera_manager_->SetColorConfig(cc);
  }
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
  
  // Initialize unified hardware panel
  hardware_panel_ = std::make_unique<HardwarePanel>();
  hardware_panel_->Initialize(bluetooth_manager_, camera_manager_, session_manager_.get(), settings_manager_.get());
  hardware_panel_->SetLogCallback([this](const std::string& message) {
    AddGlobalLog(message, LogLevel::kInfo);
  });
  
  // Initialize unified capture studio panel with all managers
  capture_studio_panel_->Initialize(camera_manager_, bluetooth_manager_, session_manager_.get());
  
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
    if (show_hardware_panel_) hardware_panel_->Render(&show_hardware_panel_);
    if (show_capture_studio_) capture_studio_panel_->Render();
    if (show_log_panel_) log_panel_->Render(&show_log_panel_);
    if (show_session_manager_) RenderSessionManagerPanel();
    if (show_network_panel_) RenderNetworkPanel();
    
    // Render dialogs
    preferences_dialog_->Show(&show_preferences_);
    if (show_about_dialog_) RenderAboutDialog();
    if (show_documentation_dialog_) RenderDocumentationDialog();
    
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
  
  // Cleanup hardware panel first (before GUI that uses it)
  if (hardware_panel_) {
    hardware_panel_->Shutdown();
    hardware_panel_.reset();
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
      ImGui::MenuItem("Hardware Control", nullptr, &show_hardware_panel_);
      ImGui::MenuItem("Capture Studio", nullptr, &show_capture_studio_);
      ImGui::MenuItem("Log", nullptr, &show_log_panel_);
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
        show_about_dialog_ = true;
      }
      if (ImGui::MenuItem("Documentation")) {
        show_documentation_dialog_ = true;
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
    
    // Split the dockspace into sections - modern layout
    auto dock_id_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.22f, nullptr, &dockspace_id);
    auto dock_id_bottom = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.25f, nullptr, &dockspace_id);
    
    // Split the remaining space between capture controls and file explorer
    auto dock_id_top = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Up, 0.4f, nullptr, &dockspace_id);
    
    // Dock windows with modern layout
    ImGui::DockBuilderDockWindow("🔧 Hardware Control", dock_id_left);
    ImGui::DockBuilderDockWindow("🎬 Capture Studio", dock_id_top);  // Top for capture controls
    ImGui::DockBuilderDockWindow("📁 File Explorer", dockspace_id);  // Main area for file explorer
    ImGui::DockBuilderDockWindow("Log", dock_id_bottom);
    ImGui::DockBuilderDockWindow("Session Manager", dock_id_bottom);
    ImGui::DockBuilderDockWindow("Network Settings", dock_id_bottom);
    
    ImGui::DockBuilderFinish(dockspace_id);
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

void Application::RenderAboutDialog() {
  ImGui::OpenPopup("About Camera Matrix Capture");

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_Appearing);

  if (ImGui::BeginPopupModal("About Camera Matrix Capture", &show_about_dialog_, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Camera Matrix Capture");
    ImGui::Separator();

    ImGui::Text("Multi-camera capture system for neural rendering datasets");
    ImGui::Spacing();

    ImGui::Text("Features:");
    ImGui::BulletText("Support for multiple Teledyne DALSA cameras");
    ImGui::BulletText("Manual and automated capture modes");
    ImGui::BulletText("Bluetooth turntable integration");
    ImGui::BulletText("Session-based file organization");
    ImGui::BulletText("Real-time camera parameter control");

    ImGui::Spacing();
    ImGui::Text("Technical Stack:");
    ImGui::BulletText("Sapera SDK for camera control");
    ImGui::BulletText("ImGui for user interface");
    ImGui::BulletText("WinRT for Bluetooth communication");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Master's thesis implementation");

    ImGui::Spacing();
    if (ImGui::Button("Close", ImVec2(120, 0))) {
      show_about_dialog_ = false;
    }

    ImGui::EndPopup();
  }
}

void Application::RenderDocumentationDialog() {
  ImGui::OpenPopup("Documentation");

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_Appearing);

  if (ImGui::BeginPopupModal("Documentation", &show_documentation_dialog_)) {
    ImGui::Text("How to Use Camera Matrix Capture");
    ImGui::Separator();

    if (ImGui::BeginTabBar("DocTabs")) {
      if (ImGui::BeginTabItem("Quick Start")) {
        ImGui::TextWrapped("Welcome! This guide will help you get started with capturing images for neural rendering:");
        ImGui::Spacing();

        ImGui::Text("1. Setup");
        ImGui::BulletText("Connect your cameras and ensure they appear in the Hardware panel");
        ImGui::BulletText("If using automated mode, connect your Bluetooth turntable");

        ImGui::Spacing();
        ImGui::Text("2. Start a Session");
        ImGui::BulletText("Go to Capture Studio panel");
        ImGui::BulletText("Enter an object name (e.g., 'chair', 'statue')");
        ImGui::BulletText("Click 'Start Session' to create a timestamped folder");

        ImGui::Spacing();
        ImGui::Text("3. Capture Images");
        ImGui::BulletText("Manual mode: Choose 'All Cameras' or 'Single Camera'");
        ImGui::BulletText("Automated mode: Set rotation angle and capture count");
        ImGui::BulletText("Click the capture button to take photos");

        ImGui::Spacing();
        ImGui::Text("4. Find Your Images");
        ImGui::BulletText("Images are saved in: neural_dataset/images/[timestamp]/");
        ImGui::BulletText("Each camera saves as: camera1_capture_001.tiff, etc.");

        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Manual Mode")) {
        ImGui::TextWrapped("Manual mode gives you full control over when to capture:");
        ImGui::Spacing();

        ImGui::Text("All Cameras Mode:");
        ImGui::BulletText("Captures from all connected cameras at once");
        ImGui::BulletText("Set capture count (1-10) for multiple shots");
        ImGui::BulletText("Perfect for static object photography");

        ImGui::Spacing();
        ImGui::Text("Single Camera Mode:");
        ImGui::BulletText("Select one specific camera from the dropdown");
        ImGui::BulletText("Great for testing individual cameras");
        ImGui::BulletText("Useful for calibration or troubleshooting");

        ImGui::Spacing();
        ImGui::Text("Tips:");
        ImGui::BulletText("Use custom names to organize your captures");
        ImGui::BulletText("Check camera status (✓ = connected, ❌ = disconnected)");
        ImGui::BulletText("Make sure session is active before capturing");

        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Automated Mode")) {
        ImGui::TextWrapped("Automated mode captures while rotating the turntable:");
        ImGui::Spacing();

        ImGui::Text("Setup Required:");
        ImGui::BulletText("Bluetooth turntable must be connected");
        ImGui::BulletText("Object should be placed on the turntable");
        ImGui::BulletText("Ensure sufficient lighting and stable setup");

        ImGui::Spacing();
        ImGui::Text("Configuration:");
        ImGui::BulletText("By Total Captures: Set how many photos (6-360)");
        ImGui::BulletText("By Angle Step: Set rotation degrees per shot (1-60°)");
        ImGui::BulletText("Turntable Speed: How fast to rotate (35-131 sec/360°)");
        ImGui::BulletText("Capture Delay: Wait time before each shot (0.5-10 sec)");

        ImGui::Spacing();
        ImGui::Text("Process:");
        ImGui::BulletText("Click 'Start' to begin automated sequence");
        ImGui::BulletText("Turntable rotates → cameras capture → repeat");
        ImGui::BulletText("Use Pause/Resume for adjustments");
        ImGui::BulletText("Click Stop to end sequence early");

        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Troubleshooting")) {
        ImGui::TextWrapped("Common issues and solutions:");
        ImGui::Spacing();

        ImGui::Text("No Cameras Connected:");
        ImGui::BulletText("Check GigE network adapter settings");
        ImGui::BulletText("Verify Sapera SDK installation");
        ImGui::BulletText("Use Hardware panel to discover/connect cameras");

        ImGui::Spacing();
        ImGui::Text("Bluetooth Issues:");
        ImGui::BulletText("Ensure turntable is powered and in pairing mode");
        ImGui::BulletText("Use Windows Bluetooth settings if needed");
        ImGui::BulletText("Check device appears in Bluetooth panel");

        ImGui::Spacing();
        ImGui::Text("Capture Problems:");
        ImGui::BulletText("Verify active session exists");
        ImGui::BulletText("Check available disk space");
        ImGui::BulletText("Ensure camera settings are valid");
        ImGui::BulletText("Try single camera mode to isolate issues");

        ImGui::Spacing();
        ImGui::Text("Performance:");
        ImGui::BulletText("Reduce capture count for faster sequences");
        ImGui::BulletText("Increase capture delay for stability");
        ImGui::BulletText("Check network bandwidth with many cameras");

        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Close", ImVec2(120, 0))) {
      show_documentation_dialog_ = false;
    }

    ImGui::EndPopup();
  }
}

}  // namespace SaperaCapturePro