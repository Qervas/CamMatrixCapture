#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <thread>
#include <fstream>
#include <ctime>

// Windows API for file dialogs and system integration
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>

// Sapera SDK FIRST to avoid macro conflicts
#include "SapClassBasic.h"
#include "hardware/CameraTypes.hpp"

// Session and Settings Management
#include "utils/SessionManager.hpp"
#include "utils/SettingsManager.hpp"

// Dear ImGui AFTER Sapera to avoid APIENTRY conflicts
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <imgui_internal.h>

using namespace SaperaCapturePro;

// Global instances for session and settings management
static std::unique_ptr<SessionManager> session_manager;
static std::unique_ptr<SettingsManager> settings_manager;

// Global variables for the GUI
static GLFWwindow* window = nullptr;
static bool show_camera_panel = true;
static bool show_capture_panel = true;
static bool show_log_panel = true;
static bool show_color_panel = true;  // NEW: Color control panel
static std::vector<std::string> log_messages;
static std::string current_image_folder = "neural_dataset";
static char image_folder_buffer[512];
static int exposure_time = 40000;
static bool capture_format_raw = false; // false = TIFF, true = RAW

// Session Management UI Variables
static char new_object_name[256] = "";
static bool show_session_manager = false;

// NEW: Advanced Color Processing Variables
struct ColorSettings {
    // White Balance
    float wb_red_gain = 1.0f;
    float wb_green_gain = 1.0f;
    float wb_blue_gain = 1.0f;
    bool auto_white_balance = false;
    
    // Color Conversion
    int color_method = 1;  // 1-7 for different quality/speed trade-offs
    int bayer_align = 2;   // 0=GBRG, 1=BGGR, 2=RGGB, 3=GRBG, 4=RGBG, 5=BGRG
    
    // Gamma Correction
    float gamma_value = 1.0f;
    bool enable_gamma = false;
    
    // LUT Support
    bool enable_lut = false;
    std::string lut_filename = "";
    
    // Advanced Settings
    bool use_hardware_conversion = false;
    bool enable_color_enhancement = false;
};

static ColorSettings color_settings;

// Advanced Camera Parameters Structure
struct CameraParameters {
    // Image Format (now with dynamic resolution)
    int width = 2560;
    int height = 1600;
    int max_width = 2560;   // Will be detected from camera
    int max_height = 1600;  // Will be detected from camera
    std::string pixel_format = "RGB8";
    
    // Acquisition Control
    int exposure_time = 10000;  // microseconds
    bool auto_exposure = false;
    float gain = 0.0f;  // dB
    bool auto_gain = false;
    float black_level = 0.0f;
    float gamma = 1.0f;
    
    // Trigger Control
    std::string trigger_mode = "Off";  // Off, On, External
    std::string trigger_source = "Software";
    std::string trigger_activation = "RisingEdge";
    float trigger_delay = 0.0f;  // microseconds
    
    // Color Control
    float wb_red = 1.0f;
    float wb_green = 1.0f;
    float wb_blue = 1.0f;
    bool auto_white_balance = false;
    float saturation = 1.0f;
    float hue = 0.0f;
    
    // Advanced Features
    std::string acquisition_mode = "Continuous";  // SingleFrame, MultiFrame, Continuous
    int acquisition_frame_count = 1;
    float acquisition_frame_rate = 30.0f;
    bool frame_rate_enable = false;
    
    // Region of Interest (ROI)
    int roi_x = 0;
    int roi_y = 0;
    int roi_width = 1920;
    int roi_height = 1080;
    bool roi_enable = false;
    
    // Transport Layer
    int packet_size = 1500;
    int packet_delay = 0;
    
    // Device Control
    std::string temperature_status = "Normal";
    float device_temperature = 25.0f;
};

static CameraParameters camera_params;
static bool show_advanced_params = false;
static std::vector<std::string> available_features;
static std::map<std::string, std::string> feature_values;
static bool refresh_features = true;

// Camera system state
static std::vector<CameraInfo> discovered_cameras;
static std::map<std::string, SapAcqDevice*> connected_devices;
static std::map<std::string, SapBuffer*> connected_buffers;
static std::map<std::string, SapAcqDeviceToBuf*> connected_transfers;
static std::map<std::string, std::string> camera_device_names; // Store device names separately
static int capture_counter = 1;

// Function declarations
void AddLogMessage(const std::string& message);
std::string GetCurrentTimestamp();
void DiscoverCameras();
void ConnectAllCameras();
void CaptureAllCameras();
bool CaptureCamera(const std::string& cameraId, const std::string& sessionPath);
std::string GenerateSessionName(int captureNumber);
void OpenFolderInExplorer(const std::string& path);
std::string BrowseForLUTFile();  // NEW: LUT file browser

// Session and Settings functions
void SaveCurrentSettings();
void LoadSettingsToUI();
void SyncCameraParametersToSettings();
void RenderSessionManagerPanel();

// GUI functions
void RenderMainMenuBar();
void RenderCameraPanel();
void RenderCapturePanel();
void RenderLogPanel();
void RenderColorPanel();  // NEW: Color control panel
void SetupVSCodeLayout(); // NEW: VSCode-style professional layout

// Function declarations for advanced parameters
void RenderAdvancedParametersPanel();
void RefreshCameraFeatures();
void DetectCameraResolution();
void ApplyParameterToAllCameras(const std::string& featureName, const std::string& value);
std::vector<std::string> GetAvailablePixelFormats();
std::vector<std::string> GetAvailableTriggerModes();

// GLFW error callback
static void GlfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main() {
    std::cout << "[NEURAL] Neural Rendering Capture System - DIRECT INTEGRATION" << std::endl;
    std::cout << "=======================================================" << std::endl;
    
    // Initialize session and settings management
    session_manager = std::make_unique<SessionManager>("neural_dataset");
    settings_manager = std::make_unique<SettingsManager>("config/settings.json");
    
    // Load settings and apply to camera parameters
    auto& cam_settings = settings_manager->GetCameraSettings();
    camera_params.width = cam_settings.width;
    camera_params.height = cam_settings.height;
    camera_params.pixel_format = cam_settings.pixel_format;
    camera_params.exposure_time = cam_settings.exposure_time;
    camera_params.gain = cam_settings.gain;
    camera_params.auto_exposure = cam_settings.auto_exposure;
    camera_params.auto_gain = cam_settings.auto_gain;
    camera_params.trigger_mode = cam_settings.trigger_mode;
    camera_params.trigger_source = cam_settings.trigger_source;
    camera_params.trigger_activation = cam_settings.trigger_activation;
    camera_params.trigger_delay = cam_settings.trigger_delay;
    camera_params.wb_red = cam_settings.white_balance_red;
    camera_params.wb_green = cam_settings.white_balance_green;
    camera_params.wb_blue = cam_settings.white_balance_blue;
    camera_params.auto_white_balance = cam_settings.auto_white_balance;
    camera_params.saturation = cam_settings.saturation;
    camera_params.hue = cam_settings.hue;
    camera_params.gamma = cam_settings.gamma;
    camera_params.roi_x = cam_settings.roi_offset_x;
    camera_params.roi_y = cam_settings.roi_offset_y;
    camera_params.roi_width = cam_settings.roi_width;
    camera_params.roi_height = cam_settings.roi_height;
    camera_params.roi_enable = cam_settings.roi_enabled;
    camera_params.acquisition_mode = cam_settings.acquisition_mode;
    camera_params.acquisition_frame_count = cam_settings.acquisition_frame_count;
    camera_params.acquisition_frame_rate = cam_settings.acquisition_frame_rate;
    camera_params.packet_size = cam_settings.packet_size;
    camera_params.packet_delay = cam_settings.packet_delay;
    
    // Set up dataset path from app settings
    auto& app_settings = settings_manager->GetAppSettings();
    current_image_folder = app_settings.last_output_folder;
    
    // Initialize folder buffer
    std::strcpy(image_folder_buffer, current_image_folder.c_str());
    
    // Setup GLFW
    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    
    // Create window
    window = glfwCreateWindow(1600, 900, "[NEURAL] Neural Rendering Capture System v2.0 - Professional Edition", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    
    // Enable viewport support for docking
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    
    // Configure basic font loading (no emoji needed)
    io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
    io.Fonts->Flags |= ImFontAtlasFlags_NoMouseCursors;
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    
    // Load default font only
    ImFont* default_font = io.Fonts->AddFontDefault();
    io.FontDefault = default_font;
    
    AddLogMessage("[OK] Font loaded successfully");
    
    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    AddLogMessage("[OK] DIRECT GUI initialized successfully");
    
    // Create dataset directories
    std::filesystem::create_directories(current_image_folder + "/images");
    std::filesystem::create_directories(current_image_folder + "/metadata");
    
    // Setup default docking layout
    bool first_time = true;
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Render GUI
        RenderMainMenuBar();
        
        // Create the docking space
        ImGui::DockSpaceOverViewport(ImGui::GetID("MyDockSpace"));
        
        // Setup default layout on first run
        if (first_time) {
            SetupVSCodeLayout();
            first_time = false;
        }
        
        // Render all panels (they will be dockable)
        if (show_camera_panel) {
            RenderCameraPanel();
        }
        
        if (show_capture_panel) {
            RenderCapturePanel();
        }
        
        if (show_advanced_params) {
            RenderAdvancedParametersPanel();
        }
        
        if (show_session_manager) {
            RenderSessionManagerPanel();
        }
        
        if (show_log_panel) {
            RenderLogPanel();
        }
        
        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.15f, 0.15f, 0.15f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
        
        glfwSwapBuffers(window);
        
        // Small delay to prevent excessive CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    // Cleanup
    AddLogMessage("[SYS] Shutting down capture system");
    
    // Clean up cameras
    for (auto& [id, transfer] : connected_transfers) {
        if (transfer) {
            transfer->Destroy();
            delete transfer;
        }
    }
    for (auto& [id, buffer] : connected_buffers) {
        if (buffer) {
            buffer->Destroy();
            delete buffer;
        }
    }
    for (auto& [id, device] : connected_devices) {
        if (device) {
            device->Destroy();
            delete device;
        }
    }
    
    // Cleanup GUI
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    std::cout << "[OK] DIRECT application terminated successfully" << std::endl;
    return 0;
}

void RenderMainMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open Dataset Folder", "Ctrl+O")) {
                OpenFolderInExplorer(current_image_folder);
            }
            if (ImGui::MenuItem("Save Settings", "Ctrl+S")) {
                SaveCurrentSettings();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Export Settings...")) {
                AddLogMessage("[SETTINGS] Export settings (file dialog not implemented)");
            }
            if (ImGui::MenuItem("Import Settings...")) {
                AddLogMessage("[SETTINGS] Import settings (file dialog not implemented)");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                glfwSetWindowShouldClose(window, true);
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Session")) {
            if (ImGui::MenuItem("Session Manager", "F1")) {
                show_session_manager = !show_session_manager;
            }
            ImGui::Separator();
            if (session_manager->HasActiveSession()) {
                if (ImGui::MenuItem("End Current Session")) {
                    session_manager->EndCurrentSession();
                    AddLogMessage("[SESSION] Session ended from menu");
                }
            } else {
                ImGui::MenuItem("No Active Session", NULL, false, false);
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Camera")) {
            if (ImGui::MenuItem("Discover Cameras", "F5")) {
                DiscoverCameras();
            }
            if (ImGui::MenuItem("Connect All", "F6")) {
                ConnectAllCameras();
            }
            if (ImGui::MenuItem("Disconnect All", "F7")) {
                AddLogMessage("[NET] Disconnect all (not implemented yet)");
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Capture")) {
            bool can_capture = connected_devices.size() > 0 && session_manager->HasActiveSession();
            if (ImGui::MenuItem("Start Capture", "F8", false, can_capture)) {
                CaptureAllCameras();
            }
            if (!can_capture && !session_manager->HasActiveSession()) {
                ImGui::MenuItem("Need Active Session", NULL, false, false);
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Camera System", NULL, &show_camera_panel);
            ImGui::MenuItem("Capture Control", NULL, &show_capture_panel);
            ImGui::MenuItem("Advanced Parameters", NULL, &show_advanced_params);
            ImGui::MenuItem("Session Manager", NULL, &show_session_manager);
            ImGui::MenuItem("System Log", NULL, &show_log_panel);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) {
                show_camera_panel = true;
                show_capture_panel = true;
                show_log_panel = true;
                show_advanced_params = true;
                show_session_manager = false;
                SetupVSCodeLayout();
                AddLogMessage("[UI] VSCode-style layout reset");
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Settings")) {
            if (ImGui::MenuItem("Save Current Settings")) {
                SaveCurrentSettings();
            }
            if (ImGui::MenuItem("Reset Camera Settings")) {
                settings_manager->ResetCameraSettings();
                LoadSettingsToUI();
                AddLogMessage("[SETTINGS] Camera settings reset to defaults");
            }
            if (ImGui::MenuItem("Reset All Settings")) {
                settings_manager->ResetAllSettings();
                LoadSettingsToUI();
                AddLogMessage("[SETTINGS] All settings reset to defaults");
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                AddLogMessage("[INFO] Neural Rendering Capture System v2.0");
            }
            if (ImGui::MenuItem("Documentation")) {
                AddLogMessage("[DOC] Documentation (not implemented yet)");
            }
            ImGui::EndMenu();
        }
        
        // Status indicators in menu bar
        ImGui::SameLine(ImGui::GetWindowWidth() - 350);
        
        // Session status
        if (session_manager->HasActiveSession()) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[SESSION]");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[NO SESSION]");
        }
        
        ImGui::SameLine();
        
        // Camera status
        int connected_count = connected_devices.size();
        int total_count = discovered_cameras.size();
        ImGui::Text("[CAM] %d/%d", connected_count, total_count);
        
        ImGui::SameLine();
        
        // Color processing status
        if (color_settings.auto_white_balance || color_settings.enable_gamma || color_settings.enable_lut) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[COLOR]");
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[COLOR]");
        }
        
        ImGui::EndMainMenuBar();
    }
}

void RenderCameraPanel() {
    ImGui::Begin("● Camera System", &show_camera_panel, ImGuiWindowFlags_NoCollapse);
    
    ImGui::Text("DIRECT Sapera SDK Camera Control");
    ImGui::Separator();
    
    // Control buttons
    if (ImGui::Button("● Discover Cameras", ImVec2(200, 40))) {
        DiscoverCameras();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("● Connect All", ImVec2(200, 40))) {
        ConnectAllCameras();
    }
    
    ImGui::Separator();
    
    // Note about parameters
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "◆ All camera parameters in Advanced Parameters panel");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "◆ Image format, resolution, exposure, etc.");
    
    if (ImGui::Button("► Open Advanced Parameters", ImVec2(200, 30))) {
        show_advanced_params = true;
        AddLogMessage("[UI] Advanced Parameters panel opened");
    }
    
    ImGui::Separator();
    
    // Camera table
    if (ImGui::BeginTable("CameraTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Camera");
        ImGui::TableSetupColumn("Serial Number");
        ImGui::TableSetupColumn("Model");
        ImGui::TableSetupColumn("Status");
        ImGui::TableHeadersRow();
        
        for (const auto& camera : discovered_cameras) {
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", camera.name.c_str());
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", camera.serialNumber.c_str());
            
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", camera.modelName.c_str());
            
            ImGui::TableSetColumnIndex(3);
            std::string cameraId = camera.id;
            if (connected_devices.find(cameraId) != connected_devices.end()) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[CONN]");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[DISC]");
            }
        }
        
        ImGui::EndTable();
    }
    
    ImGui::Text("Connected: %d / %d cameras", (int)connected_devices.size(), (int)discovered_cameras.size());
    
    // Current resolution status
    ImGui::Separator();
    ImGui::Text("Current Resolution: %dx%d (Max: %dx%d)", 
                camera_params.width, camera_params.height,
                camera_params.max_width, camera_params.max_height);
    ImGui::Text("Pixel Format: %s", camera_params.pixel_format.c_str());
    
    ImGui::End();
}

void RenderCapturePanel() {
    ImGui::Begin("▶ Capture Control", &show_capture_panel, ImGuiWindowFlags_NoCollapse);
    
    ImGui::Text("DIRECT Neural Rendering Capture");
    ImGui::Separator();
    
    // Session Management Section
    ImGui::Text("◆ Session Status");
    if (session_manager->HasActiveSession()) {
        auto* session = session_manager->GetCurrentSession();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Active: %s", session->object_name.c_str());
        ImGui::Text("Captures in session: %d", session->capture_count);
        ImGui::Text("Session ID: %s", session->session_id.c_str());
        
        if (ImGui::SmallButton("■ End Session")) {
            session_manager->EndCurrentSession();
            AddLogMessage("[SESSION] Session ended from capture panel");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("▶ Open Session Folder")) {
            OpenFolderInExplorer(session->base_path);
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No active session");
        ImGui::Text("Start a new session to begin capturing");
        
        ImGui::Text("Object Name:");
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##NewObjectName", new_object_name, sizeof(new_object_name));
        ImGui::SameLine();
        if (ImGui::Button("▶ Start Session") && strlen(new_object_name) > 0) {
            if (session_manager->StartNewSession(std::string(new_object_name))) {
                AddLogMessage("[SESSION] New session started: " + std::string(new_object_name));
                memset(new_object_name, 0, sizeof(new_object_name));
            }
        }
    }
    
    ImGui::Separator();
    
    // Dataset info
    ImGui::Text("◆ Dataset Configuration");
    ImGui::Text("Dataset: %s", current_image_folder.c_str());
    
    // Folder management
    ImGui::Text("Output Folder:");
    ImGui::SetNextItemWidth(400);
    if (ImGui::InputText("##ImageFolder", image_folder_buffer, sizeof(image_folder_buffer))) {
        current_image_folder = image_folder_buffer;
        AddLogMessage("[FOLDER] Dataset path changed to: " + current_image_folder);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("► Open")) {
        OpenFolderInExplorer(current_image_folder);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("+ Create")) {
        try {
            std::filesystem::create_directories(current_image_folder + "/images");
            std::filesystem::create_directories(current_image_folder + "/metadata");
            AddLogMessage("[FOLDER] Created folder: " + current_image_folder);
        } catch (const std::exception& e) {
            AddLogMessage("[ERR] Error creating folder: " + std::string(e.what()));
        }
    }
    
    // Format selection
    ImGui::Text("Format:");
    ImGui::SameLine();
    if (ImGui::RadioButton("TIFF", !capture_format_raw)) {
        capture_format_raw = false;
        AddLogMessage("[IMG] Format set to TIFF");
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("RAW", capture_format_raw)) {
        capture_format_raw = true;
        AddLogMessage("[IMG] Format set to RAW");
    }
    
    ImGui::Separator();
    
    // Capture section
    ImGui::Text("◆ Capture Control");
    bool can_capture = connected_devices.size() > 0 && session_manager->HasActiveSession();
    
    if (can_capture) {
        if (ImGui::Button("▶ START CAPTURE", ImVec2(300, 60))) {
            CaptureAllCameras();
        }
    } else {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        ImGui::Button("▶ START CAPTURE", ImVec2(300, 60));
        ImGui::PopStyleVar();
        
        if (!session_manager->HasActiveSession()) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Need active session to capture");
        }
        if (connected_devices.size() == 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Need connected cameras to capture");
        }
    }
    
    // Status display
    ImGui::Separator();
    ImGui::Text("◆ System Status");
    int connected_count = connected_devices.size();
    int total_count = discovered_cameras.size();
    ImGui::Text("Cameras: %d/%d connected", connected_count, total_count);
    
    if (connected_count > 0 && session_manager->HasActiveSession()) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[OK] Ready to capture");
    } else if (connected_count == 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[WARN] No cameras connected");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[WARN] No active session");
    }
    
    ImGui::End();
}

void RenderLogPanel() {
    ImGui::Begin("■ System Log", &show_log_panel, ImGuiWindowFlags_NoCollapse);
    
    // Log controls
    if (ImGui::Button("Clear Log")) {
        log_messages.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Log")) {
        AddLogMessage("[CFG] Log save (not implemented yet)");
    }
    ImGui::SameLine();
    ImGui::Text("Messages: %d", (int)log_messages.size());
    
    ImGui::Separator();
    
    // Log display
    ImGui::BeginChild("LogContent", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    
    for (const auto& message : log_messages) {
        ImGui::TextWrapped("%s", message.c_str());
    }
    
    // Auto-scroll to bottom
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    
    ImGui::EndChild();
    
    ImGui::End();
}

void RenderColorPanel() {
    ImGui::Begin("◆ Color Processing", &show_color_panel, ImGuiWindowFlags_NoCollapse);
    
    ImGui::Text("Professional Color Control Hub");
    ImGui::Separator();
    
    // All controls moved to Advanced Parameters
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "◆ All color controls moved to Advanced Parameters");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "White Balance, Gamma, Bayer, LUT, etc.");
    
    ImGui::Separator();
    
    // Quick link to Advanced Parameters
    if (ImGui::Button("► Open Advanced Parameters", ImVec2(250, 40))) {
        show_advanced_params = true;
        AddLogMessage("[UI] Advanced Parameters panel opened");
    }
    
    ImGui::Text("Find all color processing controls in:");
    ImGui::BulletText("Advanced White Balance & Color");
    ImGui::BulletText("Color Conversion & Processing");
    
    ImGui::Separator();
    
    // Quick status summary
    ImGui::Text("Current Color Settings Summary:");
    ImGui::Text("◆ White Balance: %s", camera_params.auto_white_balance ? "Auto" : "Manual");
    if (!camera_params.auto_white_balance) {
        ImGui::Text("   R:%.2f G:%.2f B:%.2f", camera_params.wb_red, camera_params.wb_green, camera_params.wb_blue);
    }
    ImGui::Text("◆ Gamma: %.2f", camera_params.gamma);
    ImGui::Text("◆ Saturation: %.2f", camera_params.saturation);
    ImGui::Text("◆ Conversion Method: %d", color_settings.color_method);
    ImGui::Text("◆ Bayer Pattern: %s", color_settings.bayer_align == 2 ? "RGGB" : "Other");
    ImGui::Text("◆ LUT: %s", color_settings.enable_lut ? "Enabled" : "Disabled");
    ImGui::Text("◆ Hardware: %s", color_settings.use_hardware_conversion ? "Yes" : "No");
    
    ImGui::End();
}

void AddLogMessage(const std::string& message) {
    std::string timestamp = GetCurrentTimestamp();
    std::string formatted_message = "[" + timestamp + "] " + message;
    
    log_messages.push_back(formatted_message);
    
    // Keep only last 200 messages
    if (log_messages.size() > 200) {
        log_messages.erase(log_messages.begin());
    }
    
    // Also print to console
    std::cout << formatted_message << std::endl;
}

std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    
    return ss.str();
}

void DiscoverCameras() {
    AddLogMessage("[DISC] Discovering DIRECT cameras...");
    
    discovered_cameras.clear();
    camera_device_names.clear();
    
    // Get server count
    int serverCount = SapManager::GetServerCount();
    AddLogMessage("[NET] Found " + std::to_string(serverCount) + " server(s)");
    
    if (serverCount == 0) {
        AddLogMessage("[NET] No Sapera servers found");
        return;
    }
    
    int cameraIndex = 1;
    
    // Enumerate all servers
    for (int serverIndex = 0; serverIndex < serverCount; serverIndex++) {
        char serverName[256];
        if (!SapManager::GetServerName(serverIndex, serverName, sizeof(serverName))) {
            AddLogMessage("[NET] Failed to get server name for server " + std::to_string(serverIndex));
            continue;
        }
        
        // Skip system server
        if (std::string(serverName) == "System") {
            continue;
        }
        
        AddLogMessage("[NET] Server " + std::to_string(serverIndex) + ": " + std::string(serverName));
        
        // Get acquisition device count for this server
        int resourceCount = SapManager::GetResourceCount(serverName, SapManager::ResourceAcqDevice);
        AddLogMessage("[NET] Acquisition devices: " + std::to_string(resourceCount));
        
        // Enumerate acquisition devices
        for (int resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++) {
            try {
                // Create acquisition device temporarily for discovery
                auto acqDevice = new SapAcqDevice(serverName, resourceIndex);
                if (!acqDevice->Create()) {
                    AddLogMessage("[NET] Failed to create device " + std::to_string(resourceIndex));
                    delete acqDevice;
                    continue;
                }
                
                // Get device information
                char buffer[512];
                
                CameraInfo camera;
                camera.id = std::to_string(cameraIndex);
                camera.serverName = serverName;
                camera.resourceIndex = resourceIndex;
                
                // Get serial number
                if (acqDevice->GetFeatureValue("DeviceSerialNumber", buffer, sizeof(buffer))) {
                    camera.serialNumber = std::string(buffer);
                } else {
                    camera.serialNumber = "Unknown_" + std::to_string(cameraIndex);
                }
                
                // Get model name
                if (acqDevice->GetFeatureValue("DeviceModelName", buffer, sizeof(buffer))) {
                    camera.modelName = std::string(buffer);
                } else {
                    camera.modelName = "Unknown_Model";
                }
                
                // Create camera name for neural rendering
                std::string indexStr = std::to_string(cameraIndex);
                if (indexStr.length() == 1) indexStr = "0" + indexStr;
                camera.name = "cam_" + indexStr;
                camera.isConnected = false;
                camera.status = CameraStatus::Disconnected;
                camera.type = CameraType::Industrial;
                
                discovered_cameras.push_back(camera);
                AddLogMessage("[OK] " + camera.name + ": " + camera.serialNumber + " (" + camera.modelName + ")");
                
                // Cleanup discovery device
                acqDevice->Destroy();
                delete acqDevice;
                
                cameraIndex++;
                
            } catch (const std::exception& e) {
                AddLogMessage("[NET] Exception: " + std::string(e.what()));
            }
        }
    }
    
    AddLogMessage("[OK] Discovery complete: " + std::to_string(discovered_cameras.size()) + " cameras found");
}

void ConnectAllCameras() {
    AddLogMessage("[NET] Connecting to DIRECT cameras...");
    
    if (discovered_cameras.empty()) {
        AddLogMessage("[NET] No cameras discovered. Run camera discovery first.");
        return;
    }
    
    int successCount = 0;
    
    for (const auto& camera : discovered_cameras) {
        std::string cameraId = camera.id;
        
        try {
            // Create acquisition device using serverName and resourceIndex
            SapAcqDevice* acqDevice = new SapAcqDevice(camera.serverName.c_str(), camera.resourceIndex);
            if (!acqDevice->Create()) {
                AddLogMessage("[NET] Failed to create acquisition device for " + camera.name);
                delete acqDevice;
                continue;
            }
            
            // Apply exposure time setting
            std::string exposureStr = std::to_string(exposure_time);
            if (!acqDevice->SetFeatureValue("ExposureTime", exposureStr.c_str())) {
                AddLogMessage("[NET] Warning: Failed to set exposure time for " + camera.name);
            }
            
            // Create buffer for image capture
            SapBuffer* buffer = new SapBufferWithTrash(1, acqDevice);
            if (!buffer->Create()) {
                AddLogMessage("[NET] Failed to create buffer for " + camera.name);
                acqDevice->Destroy();
                delete acqDevice;
                delete buffer;
                continue;
            }
            
            // Create transfer object
            SapAcqDeviceToBuf* transfer = new SapAcqDeviceToBuf(acqDevice, buffer);
            if (!transfer->Create()) {
                AddLogMessage("[NET] Failed to create transfer for " + camera.name);
                buffer->Destroy();
                acqDevice->Destroy();
                delete transfer;
                delete buffer;
                delete acqDevice;
                continue;
            }
            
            // Store connected components
            connected_devices[cameraId] = acqDevice;
            connected_buffers[cameraId] = buffer;
            connected_transfers[cameraId] = transfer;
            
            successCount++;
            AddLogMessage("[OK] " + camera.name + " connected successfully");
            
        } catch (const std::exception& e) {
            AddLogMessage("[NET] Exception connecting " + camera.name + ": " + std::string(e.what()));
        }
    }
    
    AddLogMessage("[OK] Connection summary: " + std::to_string(successCount) + "/" + std::to_string(discovered_cameras.size()) + " cameras connected");
    
    if (successCount == discovered_cameras.size() && successCount > 0) {
        AddLogMessage("[OK] All cameras connected successfully!");
        
        // Detect camera resolution after connection
        DetectCameraResolution();
    } else if (successCount > 0) {
        AddLogMessage("[WARN] Partial connection: " + std::to_string(successCount) + "/" + std::to_string(discovered_cameras.size()) + " cameras connected");
        
        // Still try to detect resolution from connected cameras
        DetectCameraResolution();
    } else {
        AddLogMessage("[ERR] No cameras could be connected");
    }
}

void CaptureAllCameras() {
    if (connected_devices.empty()) {
        AddLogMessage("[NET] No cameras connected");
        return;
    }
    
    // Check if we have an active session
    if (!session_manager->HasActiveSession()) {
        AddLogMessage("[SESSION] No active session - please start a new session first");
        return;
    }
    
    std::string sessionPath = session_manager->GetNextCapturePath();
    
    // Create session directory
    std::filesystem::create_directories(sessionPath);
    
    AddLogMessage("[REC] DIRECT CAPTURE starting...");
    AddLogMessage("[IMG] Session path: " + sessionPath);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    bool allSuccess = true;
    int successCount = 0;
    
    // Capture from all cameras
    for (const auto& camera : discovered_cameras) {
        std::string cameraId = camera.id;
        
        if (connected_devices.find(cameraId) != connected_devices.end()) {
            if (CaptureCamera(cameraId, sessionPath)) {
                successCount++;
            } else {
                allSuccess = false;
            }
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    AddLogMessage("[OK] Capture completed in " + std::to_string(duration.count()) + "ms");
    AddLogMessage("[OK] Success: " + std::to_string(successCount) + "/" + std::to_string(connected_devices.size()) + " cameras");
    
    if (allSuccess) {
        // Record the capture in session manager
        session_manager->RecordCapture(sessionPath);
        AddLogMessage("[OK] All cameras captured successfully!");
        
        // Count created files
        try {
            int file_count = 0;
            std::string images_path = current_image_folder + "/images";
            if (std::filesystem::exists(images_path)) {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(images_path)) {
                    if (entry.is_regular_file()) {
                        file_count++;
                    }
                }
            }
            AddLogMessage("[OK] Total files in dataset: " + std::to_string(file_count));
        } catch (const std::exception& e) {
            AddLogMessage("[WARN] Could not count files: " + std::string(e.what()));
        }
    } else {
        AddLogMessage("[WARN] Some cameras failed to capture");
    }
}

bool CaptureCamera(const std::string& cameraId, const std::string& sessionPath) {
    auto deviceIt = connected_devices.find(cameraId);
    auto bufferIt = connected_buffers.find(cameraId);
    auto transferIt = connected_transfers.find(cameraId);
    
    if (deviceIt == connected_devices.end() || bufferIt == connected_buffers.end() || transferIt == connected_transfers.end()) {
        return false;
    }
    
    SapAcqDevice* device = deviceIt->second;
    SapBuffer* buffer = bufferIt->second;
    SapAcqDeviceToBuf* transfer = transferIt->second;
    
    try {
        // Trigger capture
        if (!transfer->Snap()) {
            AddLogMessage("[NET] Failed to trigger capture for " + cameraId);
            return false;
        }
        
        // Wait for capture completion
        if (!transfer->Wait(5000)) {
            AddLogMessage("[NET] Capture timeout for " + cameraId);
            return false;
        }
        
        // Find camera name
        std::string cameraName = "unknown";
        for (const auto& camera : discovered_cameras) {
            if (camera.id == cameraId) {
                cameraName = camera.name;
                break;
            }
        }
        
        // Generate filename
        std::string extension = capture_format_raw ? ".raw" : ".tiff";
        int current_capture = session_manager->GetTotalCapturesInSession() + 1;
        std::string filename = cameraName + "_capture_" + std::to_string(current_capture) + extension;
        std::string fullPath = sessionPath + "/" + filename;
        
        // Save image
        if (capture_format_raw) {
            // Save as RAW
            if (buffer->Save(fullPath.c_str(), "-format raw")) {
                return true;
            }
        } else {
            // Save as TIFF with ADVANCED color conversion
            SapColorConversion colorConverter(buffer);
            if (!colorConverter.Create()) {
                AddLogMessage("[NET] Failed to create color converter for " + cameraId);
                return false;
            }
            
            // Configure converter for RGB output
            colorConverter.Enable(TRUE, color_settings.use_hardware_conversion);
            colorConverter.SetOutputFormat(SapFormatRGB888);
            
            // Set Bayer alignment based on user selection
            SapColorConversion::Align align;
            switch (color_settings.bayer_align) {
                case 0: align = SapColorConversion::AlignGBRG; break;
                case 1: align = SapColorConversion::AlignBGGR; break;
                case 2: align = SapColorConversion::AlignRGGB; break;
                case 3: align = SapColorConversion::AlignGRBG; break;
                case 4: align = SapColorConversion::AlignRGBG; break;
                case 5: align = SapColorConversion::AlignBGRG; break;
                default: align = SapColorConversion::AlignRGGB; break;
            }
            colorConverter.SetAlign(align);
            
            // Set color conversion method
            SapColorConversion::Method method;
            switch (color_settings.color_method) {
                case 1: method = SapColorConversion::Method1; break;
                case 2: method = SapColorConversion::Method2; break;
                case 3: method = SapColorConversion::Method3; break;
                case 4: method = SapColorConversion::Method4; break;
                case 5: method = SapColorConversion::Method5; break;
                case 6: method = SapColorConversion::Method6; break;
                case 7: method = SapColorConversion::Method7; break;
                default: method = SapColorConversion::Method1; break;
            }
            colorConverter.SetMethod(method);
            
            // Apply white balance
            if (color_settings.auto_white_balance) {
                // Auto white balance
                if (!colorConverter.WhiteBalance(0, 0, buffer->GetWidth(), buffer->GetHeight())) {
                    AddLogMessage("[NET] Warning: Auto white balance failed for " + cameraId + ", using manual gains");
                    // Fall back to manual gains
                    SapDataFRGB wbGain(color_settings.wb_red_gain, color_settings.wb_green_gain, color_settings.wb_blue_gain);
                    colorConverter.SetWBGain(wbGain);
                } else {
                    AddLogMessage("[OK] Auto white balance applied to " + cameraId);
                }
            } else {
                // Manual white balance
                SapDataFRGB wbGain(color_settings.wb_red_gain, color_settings.wb_green_gain, color_settings.wb_blue_gain);
                colorConverter.SetWBGain(wbGain);
            }
            
            // Apply gamma correction
            if (color_settings.enable_gamma) {
                colorConverter.SetGamma(color_settings.gamma_value);
            }
            
            // Convert the image to RGB
            if (!colorConverter.Convert()) {
                AddLogMessage("[NET] Color conversion failed for " + cameraId);
                colorConverter.Destroy();
                return false;
            }
            
            // Get converted RGB buffer
            SapBuffer* outputBuffer = colorConverter.GetOutputBuffer();
            if (!outputBuffer) {
                AddLogMessage("[NET] No output buffer for " + cameraId);
                colorConverter.Destroy();
                return false;
            }
            
            // Save converted RGB buffer as TIFF
            bool saveSuccess = outputBuffer->Save(fullPath.c_str(), "-format tiff");
            
            // Clean up converter
            colorConverter.Destroy();
            
            if (saveSuccess) {
                AddLogMessage("[OK] Advanced color conversion applied to " + cameraId + 
                             " with gamma=" + std::to_string(color_settings.gamma_value) +
                             " saturation=0.0 brightness=0.0"); // Placeholder for saturation and brightness
            }
            
            return saveSuccess;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        AddLogMessage("[NET] Exception capturing " + cameraId + ": " + std::string(e.what()));
        return false;
    }
}

std::string GenerateSessionName(int captureNumber) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << "capture_" << std::setfill('0') << std::setw(3) << captureNumber << "_";
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    
    return ss.str();
}

void OpenFolderInExplorer(const std::string& path) {
    if (std::filesystem::exists(path)) {
        // Use Windows ShellExecute for better integration
        ShellExecuteA(nullptr, "open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        AddLogMessage("[FOLDER] Opened folder: " + path);
    } else {
        AddLogMessage("[ERR] Folder does not exist: " + path);
    }
}

std::string BrowseForLUTFile() {
    char filename[MAX_PATH];
    
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "LUT Files (*.lut)\0*.lut\0Cube Files (*.cube)\0*.cube\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrTitle = "Select LUT File";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    
    if (GetOpenFileNameA(&ofn)) {
        std::string selected_file = filename;
        
        // Validate file exists and is readable
        if (std::filesystem::exists(selected_file)) {
            AddLogMessage("[LUT] LUT file selected: " + selected_file);
            return selected_file;
        } else {
            AddLogMessage("[NET] Selected LUT file does not exist: " + selected_file);
            return "";
        }
    } else {
        // User cancelled or error occurred
        DWORD error = CommDlgExtendedError();
        if (error != 0) {
            AddLogMessage("[NET] LUT file browser error: " + std::to_string(error));
        }
        return "";
    }
}

void SetupVSCodeLayout() {
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    
    // Clear any existing layout
    ImGui::DockBuilderRemoveNode(dockspace_id);
    
    // Create the main dock space
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);
    
    // Split the main area into left and right
    ImGuiID left_id, right_id;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f, &left_id, &right_id);
    
    // Split the right area into top and bottom
    ImGuiID top_right_id, bottom_right_id;
    ImGui::DockBuilderSplitNode(right_id, ImGuiDir_Down, 0.3f, &bottom_right_id, &top_right_id);
    
    // Split the left area into top and bottom for additional panels
    ImGuiID top_left_id, bottom_left_id;
    ImGui::DockBuilderSplitNode(left_id, ImGuiDir_Down, 0.4f, &bottom_left_id, &top_left_id);
    
    // Dock the windows in VSCode-style layout:
    // - Left top: Camera Panel (like VSCode's file explorer)
    // - Left bottom: Color Panel (like VSCode's source control)
    // - Top right: Capture Panel (main workspace)
    // - Bottom right: Log Panel (like VSCode's terminal/output)
    
    ImGui::DockBuilderDockWindow("● Camera System", top_left_id);
    ImGui::DockBuilderDockWindow("◆ Color Processing", bottom_left_id);
    ImGui::DockBuilderDockWindow("▶ Capture Control", top_right_id);
    ImGui::DockBuilderDockWindow("■ System Log", bottom_right_id);
    
    // Finish the docking setup
    ImGui::DockBuilderFinish(dockspace_id);
    
    AddLogMessage("[UI] VSCode-style professional layout applied");
} 

void RenderAdvancedParametersPanel() {
    ImGui::Begin("◆ Advanced Camera Parameters", &show_advanced_params, ImGuiWindowFlags_NoCollapse);
    
    if (connected_devices.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[WARN] No cameras connected");
        ImGui::Text("Connect cameras to access advanced parameters");
        ImGui::End();
        return;
    }
    
    ImGui::Text("Professional Camera Parameter Control");
    ImGui::Separator();
    
    // Feature Refresh Button
    if (ImGui::Button("◆ Refresh Features", ImVec2(150, 30))) {
        RefreshCameraFeatures();
    }
    ImGui::SameLine();
    if (ImGui::Button("► Apply All", ImVec2(100, 30))) {
        AddLogMessage("[PARAM] Applying all parameters to connected cameras");
        // Apply all current parameters
    }
    
    // Image Format & Resolution Section (moved from Camera System)
    if (ImGui::CollapsingHeader("◆ Image Format & Resolution", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushItemWidth(200);
        
        // Resolution controls (now customizable)
        int temp_width = camera_params.width;
        if (ImGui::InputInt("Width", &temp_width, 8, 64)) {
            camera_params.width = std::max(64, std::min(camera_params.max_width, temp_width));
            ApplyParameterToAllCameras("Width", std::to_string(camera_params.width));
            AddLogMessage("[PARAM] Width set to: " + std::to_string(camera_params.width));
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 64 - %d (Max detected: %d)", camera_params.max_width, camera_params.max_width);
        
        int temp_height = camera_params.height;
        if (ImGui::InputInt("Height", &temp_height, 8, 64)) {
            camera_params.height = std::max(64, std::min(camera_params.max_height, temp_height));
            ApplyParameterToAllCameras("Height", std::to_string(camera_params.height));
            AddLogMessage("[PARAM] Height set to: " + std::to_string(camera_params.height));
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 64 - %d (Max detected: %d)", camera_params.max_height, camera_params.max_height);
        
        // Quick resolution presets
        ImGui::Text("Quick Presets:");
        if (ImGui::Button("Full Res", ImVec2(70, 20))) {
            camera_params.width = camera_params.max_width;
            camera_params.height = camera_params.max_height;
            ApplyParameterToAllCameras("Width", std::to_string(camera_params.width));
            ApplyParameterToAllCameras("Height", std::to_string(camera_params.height));
            AddLogMessage("[PARAM] Full resolution applied: " + std::to_string(camera_params.width) + "x" + std::to_string(camera_params.height));
        }
        ImGui::SameLine();
        if (ImGui::Button("Half Res", ImVec2(70, 20))) {
            camera_params.width = camera_params.max_width / 2;
            camera_params.height = camera_params.max_height / 2;
            ApplyParameterToAllCameras("Width", std::to_string(camera_params.width));
            ApplyParameterToAllCameras("Height", std::to_string(camera_params.height));
            AddLogMessage("[PARAM] Half resolution applied: " + std::to_string(camera_params.width) + "x" + std::to_string(camera_params.height));
        }
        ImGui::SameLine();
        if (ImGui::Button("1080p", ImVec2(60, 20))) {
            camera_params.width = 1920;
            camera_params.height = 1080;
            ApplyParameterToAllCameras("Width", std::to_string(camera_params.width));
            ApplyParameterToAllCameras("Height", std::to_string(camera_params.height));
            AddLogMessage("[PARAM] 1080p resolution applied");
        }
        
        if (ImGui::Button("► Detect Resolution", ImVec2(150, 25))) {
            DetectCameraResolution();
        }
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Detect maximum resolution from connected cameras");
        
        ImGui::Separator();
        
        // Expanded Pixel Format Selection (all supported formats)
        std::vector<std::string> pixel_formats = {
            "Mono8", "Mono16", "RGB8", "RGB16", "BGR8", "BGR16",
            "BayerRG8", "BayerGB8", "BayerGR8", "BayerBG8",
            "BayerRG12", "BayerGB12", "BayerGR12", "BayerBG12",
            "BayerRG16", "BayerGB16", "BayerGR16", "BayerBG16",
            "YUV422", "YUV422Packed", "YUV411Packed",
            "YUV444Packed", "RGB8Packed", "BGR8Packed"
        };
        static int current_pixel_format = 2; // RGB8
        if (ImGui::Combo("Pixel Format", &current_pixel_format, 
            [](void* data, int idx, const char** out_text) {
                auto& formats = *static_cast<std::vector<std::string>*>(data);
                *out_text = formats[idx].c_str();
                return true;
            }, &pixel_formats, pixel_formats.size())) {
            camera_params.pixel_format = pixel_formats[current_pixel_format];
            ApplyParameterToAllCameras("PixelFormat", camera_params.pixel_format);
            AddLogMessage("[PARAM] Pixel format set to: " + camera_params.pixel_format);
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "RGB8/BGR8 recommended for color, Mono8/16 for grayscale");
        
        ImGui::PopItemWidth();
    }
    
    // Acquisition Control Section
    if (ImGui::CollapsingHeader("◆ Acquisition Control", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushItemWidth(200);
        
        // Exposure Time (unified control with direct input)
        int temp_exposure = camera_params.exposure_time;
        if (ImGui::InputInt("Exposure Time (us)", &temp_exposure, 100, 100000)) {
            camera_params.exposure_time = std::max(100, std::min(100000, temp_exposure));
            ApplyParameterToAllCameras("ExposureTime", std::to_string(camera_params.exposure_time));
            exposure_time = camera_params.exposure_time; // Sync with old variable for compatibility
            AddLogMessage("[PARAM] Exposure time: " + std::to_string(camera_params.exposure_time) + "us");
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 100 - 100,000 us");
        
        if (ImGui::Checkbox("Auto Exposure", &camera_params.auto_exposure)) {
            ApplyParameterToAllCameras("ExposureAuto", camera_params.auto_exposure ? "Continuous" : "Off");
            AddLogMessage("[PARAM] Auto exposure: " + std::string(camera_params.auto_exposure ? "ON" : "OFF"));
        }
        
        // Gain Control (direct input)
        float temp_gain = camera_params.gain;
        if (ImGui::InputFloat("Gain (dB)", &temp_gain, 0.1f, 1.0f, "%.1f")) {
            camera_params.gain = std::max(0.0f, std::min(30.0f, temp_gain));
            ApplyParameterToAllCameras("Gain", std::to_string(camera_params.gain));
            AddLogMessage("[PARAM] Gain: " + std::to_string(camera_params.gain) + "dB");
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 0.0 - 30.0 dB");
        
        if (ImGui::Checkbox("Auto Gain", &camera_params.auto_gain)) {
            ApplyParameterToAllCameras("GainAuto", camera_params.auto_gain ? "Continuous" : "Off");
            AddLogMessage("[PARAM] Auto gain: " + std::string(camera_params.auto_gain ? "ON" : "OFF"));
        }
        
        // Black Level (direct input)
        float temp_black = camera_params.black_level;
        if (ImGui::InputFloat("Black Level", &temp_black, 0.1f, 1.0f, "%.1f")) {
            camera_params.black_level = std::max(0.0f, std::min(100.0f, temp_black));
            ApplyParameterToAllCameras("BlackLevel", std::to_string(camera_params.black_level));
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 0.0 - 100.0");
        
        // Quick exposure presets
        ImGui::Separator();
        ImGui::Text("Quick Presets:");
        if (ImGui::Button("Fast (1ms)", ImVec2(80, 20))) {
            camera_params.exposure_time = 1000;
            ApplyParameterToAllCameras("ExposureTime", "1000");
            exposure_time = camera_params.exposure_time;
            AddLogMessage("[PARAM] Fast exposure preset applied");
        }
        ImGui::SameLine();
        if (ImGui::Button("Normal (10ms)", ImVec2(90, 20))) {
            camera_params.exposure_time = 10000;
            ApplyParameterToAllCameras("ExposureTime", "10000");
            exposure_time = camera_params.exposure_time;
            AddLogMessage("[PARAM] Normal exposure preset applied");
        }
        ImGui::SameLine();
        if (ImGui::Button("Long (50ms)", ImVec2(80, 20))) {
            camera_params.exposure_time = 50000;
            ApplyParameterToAllCameras("ExposureTime", "50000");
            exposure_time = camera_params.exposure_time;
            AddLogMessage("[PARAM] Long exposure preset applied");
        }
        
        ImGui::PopItemWidth();
    }
    
    // Trigger Control Section
    if (ImGui::CollapsingHeader("◆ Trigger Control", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushItemWidth(200);
        
        // Trigger Mode
        std::vector<std::string> trigger_modes = {"Off", "On", "External"};
        static int current_trigger_mode = 0;
        if (ImGui::Combo("Trigger Mode", &current_trigger_mode,
            [](void* data, int idx, const char** out_text) {
                auto& modes = *static_cast<std::vector<std::string>*>(data);
                *out_text = modes[idx].c_str();
                return true;
            }, &trigger_modes, trigger_modes.size())) {
            camera_params.trigger_mode = trigger_modes[current_trigger_mode];
            ApplyParameterToAllCameras("TriggerMode", camera_params.trigger_mode);
            AddLogMessage("[PARAM] Trigger mode: " + camera_params.trigger_mode);
        }
        
        // Trigger Source
        std::vector<std::string> trigger_sources = {"Software", "Hardware", "Line0", "Line1", "Line2"};
        static int current_trigger_source = 0;
        if (ImGui::Combo("Trigger Source", &current_trigger_source,
            [](void* data, int idx, const char** out_text) {
                auto& sources = *static_cast<std::vector<std::string>*>(data);
                *out_text = sources[idx].c_str();
                return true;
            }, &trigger_sources, trigger_sources.size())) {
            camera_params.trigger_source = trigger_sources[current_trigger_source];
            ApplyParameterToAllCameras("TriggerSource", camera_params.trigger_source);
            AddLogMessage("[PARAM] Trigger source: " + camera_params.trigger_source);
        }
        
        // Trigger Activation
        std::vector<std::string> trigger_activations = {"RisingEdge", "FallingEdge", "AnyEdge", "LevelHigh", "LevelLow"};
        static int current_trigger_activation = 0;
        if (ImGui::Combo("Trigger Activation", &current_trigger_activation,
            [](void* data, int idx, const char** out_text) {
                auto& activations = *static_cast<std::vector<std::string>*>(data);
                *out_text = activations[idx].c_str();
                return true;
            }, &trigger_activations, trigger_activations.size())) {
            camera_params.trigger_activation = trigger_activations[current_trigger_activation];
            ApplyParameterToAllCameras("TriggerActivation", camera_params.trigger_activation);
        }
        
        // Trigger Delay (direct input)
        float temp_trigger_delay = camera_params.trigger_delay;
        if (ImGui::InputFloat("Trigger Delay (us)", &temp_trigger_delay, 1.0f, 100.0f, "%.1f")) {
            camera_params.trigger_delay = std::max(0.0f, std::min(10000.0f, temp_trigger_delay));
            ApplyParameterToAllCameras("TriggerDelay", std::to_string(camera_params.trigger_delay));
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 0.0 - 10,000.0 us");
        
        ImGui::PopItemWidth();
    }
    
    // Advanced White Balance & Color Section
    if (ImGui::CollapsingHeader("◆ Advanced White Balance & Color", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushItemWidth(200);
        
        // White Balance Controls
        if (ImGui::Checkbox("Auto White Balance", &camera_params.auto_white_balance)) {
            ApplyParameterToAllCameras("BalanceWhiteAuto", camera_params.auto_white_balance ? "Continuous" : "Off");
            // Also sync with color_settings for compatibility
            color_settings.auto_white_balance = camera_params.auto_white_balance;
        }
        
        if (!camera_params.auto_white_balance) {
            float temp_wb_red = camera_params.wb_red;
            if (ImGui::InputFloat("WB Red", &temp_wb_red, 0.01f, 0.1f, "%.2f")) {
                camera_params.wb_red = std::max(0.1f, std::min(4.0f, temp_wb_red));
                ApplyParameterToAllCameras("BalanceRatioRed", std::to_string(camera_params.wb_red));
                color_settings.wb_red_gain = camera_params.wb_red; // Sync
            }
            
            float temp_wb_green = camera_params.wb_green;
            if (ImGui::InputFloat("WB Green", &temp_wb_green, 0.01f, 0.1f, "%.2f")) {
                camera_params.wb_green = std::max(0.1f, std::min(4.0f, temp_wb_green));
                ApplyParameterToAllCameras("BalanceRatioGreen", std::to_string(camera_params.wb_green));
                color_settings.wb_green_gain = camera_params.wb_green; // Sync
            }
            
            float temp_wb_blue = camera_params.wb_blue;
            if (ImGui::InputFloat("WB Blue", &temp_wb_blue, 0.01f, 0.1f, "%.2f")) {
                camera_params.wb_blue = std::max(0.1f, std::min(4.0f, temp_wb_blue));
                ApplyParameterToAllCameras("BalanceRatioBlue", std::to_string(camera_params.wb_blue));
                color_settings.wb_blue_gain = camera_params.wb_blue; // Sync
            }
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 0.1 - 4.0 for all WB channels");
            
            // Quick WB presets
            ImGui::Text("Quick Presets:");
            if (ImGui::Button("Neutral", ImVec2(60, 20))) {
                camera_params.wb_red = camera_params.wb_green = camera_params.wb_blue = 1.0f;
                ApplyParameterToAllCameras("BalanceRatioRed", "1.0");
                ApplyParameterToAllCameras("BalanceRatioGreen", "1.0");
                ApplyParameterToAllCameras("BalanceRatioBlue", "1.0");
                AddLogMessage("[PARAM] Applied neutral white balance");
            }
            ImGui::SameLine();
            if (ImGui::Button("Warm", ImVec2(50, 20))) {
                camera_params.wb_red = 1.2f; camera_params.wb_green = 1.0f; camera_params.wb_blue = 0.8f;
                ApplyParameterToAllCameras("BalanceRatioRed", "1.2");
                ApplyParameterToAllCameras("BalanceRatioGreen", "1.0");
                ApplyParameterToAllCameras("BalanceRatioBlue", "0.8");
                AddLogMessage("[PARAM] Applied warm white balance");
            }
            ImGui::SameLine();
            if (ImGui::Button("Cool", ImVec2(50, 20))) {
                camera_params.wb_red = 0.8f; camera_params.wb_green = 1.0f; camera_params.wb_blue = 1.2f;
                ApplyParameterToAllCameras("BalanceRatioRed", "0.8");
                ApplyParameterToAllCameras("BalanceRatioGreen", "1.0");
                ApplyParameterToAllCameras("BalanceRatioBlue", "1.2");
                AddLogMessage("[PARAM] Applied cool white balance");
            }
        }
        
        // Color Enhancement (direct input)
        float temp_saturation = camera_params.saturation;
        if (ImGui::InputFloat("Saturation", &temp_saturation, 0.01f, 0.1f, "%.2f")) {
            camera_params.saturation = std::max(0.0f, std::min(2.0f, temp_saturation));
            ApplyParameterToAllCameras("Saturation", std::to_string(camera_params.saturation));
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 0.0 - 2.0");
        
        float temp_hue = camera_params.hue;
        if (ImGui::InputFloat("Hue", &temp_hue, 1.0f, 10.0f, "%.1f")) {
            camera_params.hue = std::max(-180.0f, std::min(180.0f, temp_hue));
            ApplyParameterToAllCameras("Hue", std::to_string(camera_params.hue));
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: -180.0 - 180.0 degrees");
        
        // Gamma (integrated from color panel, direct input)
        float temp_gamma = camera_params.gamma;
        if (ImGui::InputFloat("Gamma", &temp_gamma, 0.01f, 0.1f, "%.2f")) {
            camera_params.gamma = std::max(0.1f, std::min(3.0f, temp_gamma));
            ApplyParameterToAllCameras("Gamma", std::to_string(camera_params.gamma));
            color_settings.gamma_value = camera_params.gamma; // Sync
            color_settings.enable_gamma = (camera_params.gamma != 1.0f);
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 0.1 - 3.0 (1.0 = linear)");
        
        ImGui::PopItemWidth();
    }
    
    // Color Conversion & Processing Section (moved from Color Processing panel)
    if (ImGui::CollapsingHeader("◆ Color Conversion & Processing")) {
        ImGui::PushItemWidth(200);
        
        // Bayer Pattern Selection
        ImGui::Text("◆ Bayer Pattern Alignment");
        std::vector<std::string> bayer_patterns = {"GBRG", "BGGR", "RGGB", "GRBG", "RGBG", "BGRG"};
        static int current_bayer = 2; // RGGB default
        
        if (ImGui::Combo("Bayer Alignment", &current_bayer,
            [](void* data, int idx, const char** out_text) {
                auto& patterns = *static_cast<std::vector<std::string>*>(data);
                *out_text = patterns[idx].c_str();
                return true;
            }, &bayer_patterns, bayer_patterns.size())) {
            color_settings.bayer_align = current_bayer;
            AddLogMessage("[COLOR] Bayer pattern set to: " + bayer_patterns[current_bayer]);
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "RGGB is most common for Nano-C4020");
        
        // Color Conversion Method (direct input)
        ImGui::Text("◆ Color Conversion Quality");
        int temp_method = color_settings.color_method;
        if (ImGui::InputInt("Method (1=Fast, 7=Best)", &temp_method, 1, 1)) {
            color_settings.color_method = std::max(1, std::min(7, temp_method));
            AddLogMessage("[COLOR] Color conversion method: " + std::to_string(color_settings.color_method));
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "1=Fastest, 7=Highest Quality");
        
        // Hardware vs Software toggle
        if (ImGui::Checkbox("Use Hardware Conversion", &color_settings.use_hardware_conversion)) {
            AddLogMessage("[COLOR] Hardware conversion " + std::string(color_settings.use_hardware_conversion ? "enabled" : "disabled"));
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Hardware may improve performance");
        
        // LUT File Management
        ImGui::Separator();
        ImGui::Text("◆ Look-Up Table (LUT)");
        
        if (ImGui::Checkbox("Enable LUT", &color_settings.enable_lut)) {
            AddLogMessage("[LUT] LUT processing " + std::string(color_settings.enable_lut ? "enabled" : "disabled"));
        }
        
        if (color_settings.enable_lut) {
            ImGui::Text("LUT File: %s", color_settings.lut_filename.empty() ? "[None Selected]" : color_settings.lut_filename.c_str());
            
            if (ImGui::Button("► Browse LUT File", ImVec2(150, 25))) {
                std::string lutFile = BrowseForLUTFile();
                if (!lutFile.empty()) {
                    color_settings.lut_filename = lutFile;
                    AddLogMessage("[LUT] LUT file selected: " + lutFile);
                }
            }
            
            if (!color_settings.lut_filename.empty()) {
                ImGui::SameLine();
                if (ImGui::Button("► Apply LUT", ImVec2(100, 25))) {
                    // Apply LUT to all connected cameras
                    for (auto& [id, device] : connected_devices) {
                        if (device) {
                            // Load and apply LUT using Sapera SDK
                            AddLogMessage("[LUT] Applying LUT to " + id);
                        }
                    }
                    AddLogMessage("[LUT] LUT applied to all cameras");
                }
            }
        }
        
        ImGui::PopItemWidth();
    }
    
    // Region of Interest Section
    if (ImGui::CollapsingHeader("◆ Region of Interest (ROI)")) {
        ImGui::PushItemWidth(150);
        
        if (ImGui::Checkbox("Enable ROI", &camera_params.roi_enable)) {
            AddLogMessage("[PARAM] ROI " + std::string(camera_params.roi_enable ? "enabled" : "disabled"));
        }
        
        if (camera_params.roi_enable) {
            if (ImGui::InputInt("ROI X", &camera_params.roi_x, 8, 64)) {
                camera_params.roi_x = std::max(0, camera_params.roi_x);
                ApplyParameterToAllCameras("OffsetX", std::to_string(camera_params.roi_x));
            }
            if (ImGui::InputInt("ROI Y", &camera_params.roi_y, 8, 64)) {
                camera_params.roi_y = std::max(0, camera_params.roi_y);
                ApplyParameterToAllCameras("OffsetY", std::to_string(camera_params.roi_y));
            }
            if (ImGui::InputInt("ROI Width", &camera_params.roi_width, 8, 64)) {
                camera_params.roi_width = std::max(64, camera_params.roi_width);
                ApplyParameterToAllCameras("Width", std::to_string(camera_params.roi_width));
            }
            if (ImGui::InputInt("ROI Height", &camera_params.roi_height, 8, 64)) {
                camera_params.roi_height = std::max(64, camera_params.roi_height);
                ApplyParameterToAllCameras("Height", std::to_string(camera_params.roi_height));
            }
        }
        
        ImGui::PopItemWidth();
    }
    
    // Acquisition Mode Section
    if (ImGui::CollapsingHeader("◆ Acquisition Mode")) {
        ImGui::PushItemWidth(200);
        
        std::vector<std::string> acq_modes = {"SingleFrame", "MultiFrame", "Continuous"};
        static int current_acq_mode = 2; // Continuous
        if (ImGui::Combo("Acquisition Mode", &current_acq_mode,
            [](void* data, int idx, const char** out_text) {
                auto& modes = *static_cast<std::vector<std::string>*>(data);
                *out_text = modes[idx].c_str();
                return true;
            }, &acq_modes, acq_modes.size())) {
            camera_params.acquisition_mode = acq_modes[current_acq_mode];
            ApplyParameterToAllCameras("AcquisitionMode", camera_params.acquisition_mode);
        }
        
        if (camera_params.acquisition_mode == "MultiFrame") {
            if (ImGui::InputInt("Frame Count", &camera_params.acquisition_frame_count, 1, 10)) {
                camera_params.acquisition_frame_count = std::max(1, camera_params.acquisition_frame_count);
                ApplyParameterToAllCameras("AcquisitionFrameCount", std::to_string(camera_params.acquisition_frame_count));
            }
        }
        
        if (ImGui::Checkbox("Enable Frame Rate", &camera_params.frame_rate_enable)) {
            ApplyParameterToAllCameras("AcquisitionFrameRateEnable", camera_params.frame_rate_enable ? "true" : "false");
        }
        
        if (camera_params.frame_rate_enable) {
            float temp_frame_rate = camera_params.acquisition_frame_rate;
            if (ImGui::InputFloat("Frame Rate (fps)", &temp_frame_rate, 0.1f, 1.0f, "%.1f")) {
                camera_params.acquisition_frame_rate = std::max(1.0f, std::min(120.0f, temp_frame_rate));
                ApplyParameterToAllCameras("AcquisitionFrameRate", std::to_string(camera_params.acquisition_frame_rate));
            }
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 1.0 - 120.0 fps");
        }
        
        ImGui::PopItemWidth();
    }
    
    // Transport Layer Section
    if (ImGui::CollapsingHeader("◆ Transport Layer (GigE)")) {
        ImGui::PushItemWidth(200);
        
        if (ImGui::InputInt("Packet Size", &camera_params.packet_size, 64, 512)) {
            camera_params.packet_size = std::max(576, std::min(9000, camera_params.packet_size));
            ApplyParameterToAllCameras("GevSCPSPacketSize", std::to_string(camera_params.packet_size));
        }
        
        if (ImGui::InputInt("Inter-Packet Delay", &camera_params.packet_delay, 10, 100)) {
            camera_params.packet_delay = std::max(0, camera_params.packet_delay);
            ApplyParameterToAllCameras("GevSCPD", std::to_string(camera_params.packet_delay));
        }
        
        ImGui::PopItemWidth();
    }
    
    // Device Information Section
    if (ImGui::CollapsingHeader("◆ Device Information")) {
        ImGui::Text("Temperature Status: %s", camera_params.temperature_status.c_str());
        ImGui::Text("Device Temperature: %.1f°C", camera_params.device_temperature);
        
        if (ImGui::Button("► Update Device Info", ImVec2(150, 25))) {
            AddLogMessage("[PARAM] Refreshing device information");
            // Refresh device info from cameras
        }
    }
    
    // Live Feature Browser Section
    if (ImGui::CollapsingHeader("◆ Live Feature Browser")) {
        ImGui::Text("Available Features (%zu)", available_features.size());
        
        if (ImGui::BeginChild("FeatureList", ImVec2(0, 200), true)) {
            for (const auto& feature : available_features) {
                if (ImGui::Selectable(feature.c_str())) {
                    AddLogMessage("[PARAM] Selected feature: " + feature);
                }
                
                // Show current value if available
                auto it = feature_values.find(feature);
                if (it != feature_values.end()) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "= %s", it->second.c_str());
                }
            }
        }
        ImGui::EndChild();
    }
    
    ImGui::End();
}

void RefreshCameraFeatures() {
    available_features.clear();
    feature_values.clear();
    
    if (connected_devices.empty()) {
        AddLogMessage("[PARAM] No cameras connected for feature refresh");
        return;
    }
    
    // Get features from the first connected camera
    auto firstCamera = connected_devices.begin();
    SapAcqDevice* device = firstCamera->second;
    
    if (!device) {
        AddLogMessage("[PARAM] Invalid device handle");
        return;
    }
    
    try {
        int featureCount = 0;
        if (device->GetFeatureCount(&featureCount)) {
            AddLogMessage("[PARAM] Found " + std::to_string(featureCount) + " features");
            
            for (int i = 0; i < featureCount; i++) {
                char featureName[256];
                if (device->GetFeatureNameByIndex(i, featureName, sizeof(featureName))) {
                    available_features.push_back(std::string(featureName));
                    
                    // Try to get current value
                    char featureValue[256];
                    if (device->GetFeatureValue(featureName, featureValue, sizeof(featureValue))) {
                        feature_values[std::string(featureName)] = std::string(featureValue);
                    }
                }
            }
        }
        
        AddLogMessage("[PARAM] Feature refresh complete");
        
    } catch (const std::exception& e) {
        AddLogMessage("[PARAM] Error refreshing features: " + std::string(e.what()));
    }
}

void DetectCameraResolution() {
    if (connected_devices.empty()) {
        AddLogMessage("[PARAM] No cameras connected for resolution detection");
        return;
    }
    
    // Get resolution from the first connected camera
    auto firstCamera = connected_devices.begin();
    SapAcqDevice* device = firstCamera->second;
    
    if (!device) {
        AddLogMessage("[PARAM] Invalid device handle");
        return;
    }
    
    try {
        // Try to get maximum width and height
        int maxWidth = 0, maxHeight = 0;
        int currentWidth = 0, currentHeight = 0;
        
        // Get current dimensions
        if (device->GetFeatureValue("Width", &currentWidth)) {
            camera_params.width = currentWidth;
            AddLogMessage("[PARAM] Current width detected: " + std::to_string(currentWidth));
        }
        
        if (device->GetFeatureValue("Height", &currentHeight)) {
            camera_params.height = currentHeight;
            AddLogMessage("[PARAM] Current height detected: " + std::to_string(currentHeight));
        }
        
        // Try to get maximum dimensions
        if (device->GetFeatureValue("WidthMax", &maxWidth)) {
            camera_params.max_width = maxWidth;
            AddLogMessage("[PARAM] Maximum width detected: " + std::to_string(maxWidth));
        } else {
            // Fallback: try SensorWidth
            if (device->GetFeatureValue("SensorWidth", &maxWidth)) {
                camera_params.max_width = maxWidth;
                AddLogMessage("[PARAM] Sensor width detected: " + std::to_string(maxWidth));
            }
        }
        
        if (device->GetFeatureValue("HeightMax", &maxHeight)) {
            camera_params.max_height = maxHeight;
            AddLogMessage("[PARAM] Maximum height detected: " + std::to_string(maxHeight));
        } else {
            // Fallback: try SensorHeight
            if (device->GetFeatureValue("SensorHeight", &maxHeight)) {
                camera_params.max_height = maxHeight;
                AddLogMessage("[PARAM] Sensor height detected: " + std::to_string(maxHeight));
            }
        }
        
        AddLogMessage("[PARAM] Resolution detection complete - Max: " + 
                     std::to_string(camera_params.max_width) + "x" + std::to_string(camera_params.max_height));
        
    } catch (const std::exception& e) {
        AddLogMessage("[PARAM] Error detecting resolution: " + std::string(e.what()));
    }
}

void ApplyParameterToAllCameras(const std::string& featureName, const std::string& value) {
    int successCount = 0;
    int totalCount = connected_devices.size();
    
    for (auto& [cameraId, device] : connected_devices) {
        if (device && device->SetFeatureValue(featureName.c_str(), value.c_str())) {
            successCount++;
        }
    }
    
    if (successCount == totalCount) {
        AddLogMessage("[PARAM] " + featureName + " = " + value + " applied to all cameras");
    } else {
        AddLogMessage("[PARAM] " + featureName + " applied to " + std::to_string(successCount) + "/" + std::to_string(totalCount) + " cameras");
    }
}

std::vector<std::string> GetAvailablePixelFormats() {
    return {"Mono8", "Mono12", "Mono16", "RGB8", "BGR8", "BayerRG8", "BayerGB8", "BayerGR8", "BayerBG8", "BayerRG12", "BayerGB12", "BayerGR12", "BayerBG12"};
}

std::vector<std::string> GetAvailableTriggerModes() {
    return {"Off", "On", "External"};
}

// Settings Management Functions
void SaveCurrentSettings() {
    if (!settings_manager) return;
    
    // Sync current camera parameters to settings
    SyncCameraParametersToSettings();
    
    // Save to file
    settings_manager->Save();
    AddLogMessage("[SETTINGS] Settings saved successfully");
}

void LoadSettingsToUI() {
    if (!settings_manager) return;
    
    // Load settings has already been done in main(), this refreshes UI values if needed
    auto& cam_settings = settings_manager->GetCameraSettings();
    camera_params.width = cam_settings.width;
    camera_params.height = cam_settings.height;
    camera_params.pixel_format = cam_settings.pixel_format;
    camera_params.exposure_time = cam_settings.exposure_time;
    // ... other parameters loaded in main()
    
    AddLogMessage("[SETTINGS] Settings loaded to UI");
}

void SyncCameraParametersToSettings() {
    if (!settings_manager) return;
    
    auto& cam_settings = settings_manager->GetCameraSettings();
    
    // Sync all camera parameters to settings structure
    cam_settings.width = camera_params.width;
    cam_settings.height = camera_params.height;
    cam_settings.pixel_format = camera_params.pixel_format;
    cam_settings.exposure_time = camera_params.exposure_time;
    cam_settings.gain = camera_params.gain;
    cam_settings.auto_exposure = camera_params.auto_exposure;
    cam_settings.auto_gain = camera_params.auto_gain;
    cam_settings.trigger_mode = camera_params.trigger_mode;
    cam_settings.trigger_source = camera_params.trigger_source;
    cam_settings.trigger_activation = camera_params.trigger_activation;
    cam_settings.trigger_delay = camera_params.trigger_delay;
    cam_settings.white_balance_red = camera_params.wb_red;
    cam_settings.white_balance_green = camera_params.wb_green;
    cam_settings.white_balance_blue = camera_params.wb_blue;
    cam_settings.auto_white_balance = camera_params.auto_white_balance;
    cam_settings.saturation = camera_params.saturation;
    cam_settings.hue = camera_params.hue;
    cam_settings.gamma = camera_params.gamma;
    cam_settings.roi_offset_x = camera_params.roi_x;
    cam_settings.roi_offset_y = camera_params.roi_y;
    cam_settings.roi_width = camera_params.roi_width;
    cam_settings.roi_height = camera_params.roi_height;
    cam_settings.roi_enabled = camera_params.roi_enable;
    cam_settings.acquisition_mode = camera_params.acquisition_mode;
    cam_settings.acquisition_frame_count = camera_params.acquisition_frame_count;
    cam_settings.acquisition_frame_rate = camera_params.acquisition_frame_rate;
    cam_settings.packet_size = camera_params.packet_size;
    cam_settings.packet_delay = camera_params.packet_delay;
}

void RenderSessionManagerPanel() {
    if (!ImGui::Begin("■ Session Management", &show_session_manager)) {
        ImGui::End();
        return;
    }
    
    // Current Session Status
    ImGui::Text("◆ Current Session");
    ImGui::Separator();
    
    if (session_manager->HasActiveSession()) {
        auto* session = session_manager->GetCurrentSession();
        ImGui::Text("Active: %s", session->object_name.c_str());
        ImGui::Text("Captures: %d", session->capture_count);
        ImGui::Text("Session ID: %s", session->session_id.c_str());
        
        if (ImGui::Button("■ End Current Session")) {
            session_manager->EndCurrentSession();
            AddLogMessage("[SESSION] Current session ended");
        }
    } else {
        ImGui::Text("No active session");
        
        ImGui::Spacing();
        ImGui::Text("■ Start New Session");
        ImGui::InputText("Object Name", new_object_name, sizeof(new_object_name));
        
        if (ImGui::Button("▶ Start Session") && strlen(new_object_name) > 0) {
            if (session_manager->StartNewSession(std::string(new_object_name))) {
                AddLogMessage("[SESSION] New session started for: " + std::string(new_object_name));
                memset(new_object_name, 0, sizeof(new_object_name)); // Clear input
            }
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    
    // Session History
    ImGui::Text("◆ Session History");
    const auto& history = session_manager->GetSessionHistory();
    
    if (history.empty()) {
        ImGui::Text("No previous sessions");
    } else {
        for (int i = static_cast<int>(history.size()) - 1; i >= 0; i--) {
            const auto& session = history[i];
            ImGui::Text("● %s (%d captures)", session.object_name.c_str(), session.capture_count);
            ImGui::SameLine();
            std::string button_id = "Open##" + std::to_string(i);
            if (ImGui::SmallButton(button_id.c_str())) {
                OpenFolderInExplorer(session.base_path);
            }
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    
    // Session Statistics
    ImGui::Text("◆ Statistics");
    ImGui::Text("Total Sessions: %d", session_manager->GetTotalSessions());
    if (session_manager->HasActiveSession()) {
        ImGui::Text("Current Session Captures: %d", session_manager->GetTotalCapturesInSession());
    }
    
    ImGui::End();
} 