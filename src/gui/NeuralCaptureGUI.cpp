#include "gui/NeuralCaptureGUI.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

// Constructor
NeuralCaptureGUI::NeuralCaptureGUI() {
    current_image_folder_ = "neural_dataset";
    std::strcpy(image_folder_buffer_, current_image_folder_.c_str());

    // Initialize REAL capture system
    try {
        capture_system_ = std::make_unique<NeuralRenderingCaptureSystem>(current_image_folder_);
        AddLogMessage("✅ Capture system initialized successfully.", "INFO");
    } catch (const std::exception& e) {
        AddLogMessage("❌ Failed to initialize capture system: " + std::string(e.what()), "ERROR");
        capture_system_ = nullptr;
    }

    AddLogMessage("🎬 Neural Capture GUI constructed.", "INFO");
}

// Destructor
NeuralCaptureGUI::~NeuralCaptureGUI() {
    // Shutdown is called from main
}

// Initialize
bool NeuralCaptureGUI::Initialize() {
    // Setup GLFW
    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Create window
    window_ = glfwCreateWindow(1800, 1000, "Neural Rendering Capture System", NULL, NULL);
    if (window_ == nullptr) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style
    SetupImGuiStyle();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    AddLogMessage("✅ GUI initialized successfully.", "SUCCESS");
    return true;
}

// Run loop
void NeuralCaptureGUI::Run() {
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Render GUI elements
        RenderMainMenuBar();
        RenderPanels();
        RenderStatusBar();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color_.x, clear_color_.y, clear_color_.z, clear_color_.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);
    }
}

// Shutdown
void NeuralCaptureGUI::Shutdown() {
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();

    AddLogMessage("🛑 Application shutdown complete.", "INFO");
}

void NeuralCaptureGUI::GlfwErrorCallback(int error, const char* description) {
    std::cerr << "Glfw Error " << error << ": " << description << std::endl;
}

void NeuralCaptureGUI::SetupImGuiStyle() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(5.0f, 4.0f);
    style.ItemSpacing = ImVec2(6.0f, 5.0f);
    style.ItemInnerSpacing = ImVec2(5.0f, 5.0f);
    style.WindowRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 4.0f;
}

void NeuralCaptureGUI::RenderMainMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                glfwSetWindowShouldClose(window_, true);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Camera Panel", NULL, &show_camera_panel_);
            ImGui::MenuItem("Parameter Panel", NULL, &show_parameter_panel_);
            ImGui::MenuItem("Capture Panel", NULL, &show_capture_panel_);
            ImGui::MenuItem("System Log", NULL, &show_log_panel_);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void NeuralCaptureGUI::RenderPanels() {
    // A simple docking setup can be added here if needed in the future
    if (show_camera_panel_) RenderCameraPanel();
    if (show_parameter_panel_) RenderParameterPanel();
    if (show_capture_panel_) RenderCapturePanel();
    if (show_log_panel_) RenderLogPanel();
}


void NeuralCaptureGUI::RenderCameraPanel() {
    ImGui::Begin("Camera System", &show_camera_panel_);

    ImGui::Text("Sapera SDK Camera Control");
    ImGui::Separator();

    // Control buttons
    if (ImGui::Button("🔍 Discover Cameras", ImVec2(150, 35))) {
        DiscoverCameras();
    }

    ImGui::SameLine();
    if (ImGui::Button("🔌 Connect All", ImVec2(150, 35))) {
        ConnectAllCameras();
    }
    


    ImGui::SameLine();
    if (ImGui::Button("📊 Status", ImVec2(150, 35))) {
        ShowCameraStatus();
    }

    ImGui::Separator();

    // Camera table
    if (capture_system_) {
        const auto& discovered = capture_system_->getDiscoveredCameras();
        
        if (ImGui::BeginTable("CameraTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Camera Name");
            ImGui::TableSetupColumn("Serial Number");
            ImGui::TableSetupColumn("Model");
            ImGui::TableSetupColumn("Status");
            ImGui::TableHeadersRow();

            for (const auto& camera : discovered) {
                ImGui::TableNextRow();
                
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", camera.name.c_str());
                
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", camera.serialNumber.c_str());
                
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s", camera.modelName.c_str());
                
                ImGui::TableSetColumnIndex(3);
                if (camera.isConnected) {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "🟢 Connected");
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "🔴 Disconnected");
                }
            }
            ImGui::EndTable();
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "❌ Capture system not initialized");
    }

    ImGui::End();
}


void NeuralCaptureGUI::RenderParameterPanel() {
    ImGui::Begin("Parameter Control", &show_parameter_panel_);
    ImGui::Text("Camera settings and parameters will be displayed here.");
    if (capture_system_) {
        int exposure = capture_system_->getExposureTime();
        if (ImGui::SliderInt("Exposure Time (us)", &exposure, 10, 100000)) {
            if (capture_system_->setExposureTime(exposure)) {
                 AddLogMessage("Set exposure to " + std::to_string(exposure) + " us", "INFO");
            } else {
                 AddLogMessage("Failed to set exposure time.", "WARNING");
            }
        }
    } else {
        ImGui::Text("Capture system not ready.");
    }
    ImGui::End();
}

void NeuralCaptureGUI::RenderCapturePanel() {
    ImGui::Begin("Capture Control", &show_capture_panel_);
    ImGui::Text("Capture session management and controls.");
    ImGui::Separator();

    if (capture_system_) {
        ImGui::InputText("Dataset Folder", image_folder_buffer_, sizeof(image_folder_buffer_));
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            current_image_folder_ = image_folder_buffer_;
            capture_system_->setDatasetPath(current_image_folder_);
            AddLogMessage("Dataset path set to: " + current_image_folder_, "INFO");
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Open")) {
            OpenFolderInExplorer(current_image_folder_);
        }

        const char* formats[] = { "TIFF", "RAW" };
        int current_format_idx = (capture_system_->getCurrentFormat() == CaptureFormat::TIFF) ? 0 : 1;
        if (ImGui::Combo("Format", &current_format_idx, formats, IM_ARRAYSIZE(formats))) {
            capture_system_->setFormat(current_format_idx == 0 ? CaptureFormat::TIFF : CaptureFormat::RAW);
            AddLogMessage("Format set to " + std::string(formats[current_format_idx]), "INFO");
        }

        ImGui::Separator();

        if (ImGui::Button("📸 CAPTURE ALL", ImVec2(-1, 50))) {
            CaptureAllCameras();
        }

        ImGui::Text("Capture count: %d", capture_system_->getCaptureCounter());
        ImGui::SameLine();
        if (ImGui::Button("Reset Counter")) {
            capture_system_->resetCaptureCounter();
            AddLogMessage("Capture counter reset.", "INFO");
        }

    } else {
        ImGui::Text("Capture system not ready.");
    }
    ImGui::End();
}

void NeuralCaptureGUI::RenderLogPanel() {
    ImGui::Begin("System Log", &show_log_panel_);
    
    if (ImGui::Button("Clear Log")) {
        log_messages_.clear();
    }
    ImGui::SameLine();
    static bool auto_scroll = true;
    ImGui::Checkbox("Auto-scroll", &auto_scroll);
    
    ImGui::Separator();

    ImGui::BeginChild("LogScrollingRegion", ImVec2(0,0), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& msg : log_messages_) {
        ImGui::TextUnformatted(msg.c_str());
    }
    
    if (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    
    ImGui::EndChild();
    ImGui::End();
}

void NeuralCaptureGUI::RenderStatusBar() {
    // We can add a status bar at the bottom of the window in the future
}


// System Operations
void NeuralCaptureGUI::DiscoverCameras() {
    if (!capture_system_) {
        AddLogMessage("❌ Cannot discover, capture system is not initialized.", "ERROR");
        return;
    }
    AddLogMessage("🔍 Discovering cameras...", "INFO");
    const auto& cameras = capture_system_->discoverCameras();
    std::string msg = "✅ Discovered " + std::to_string(cameras.size()) + " cameras.";
    AddLogMessage(msg, "SUCCESS");
    ShowCameraStatus();
}

void NeuralCaptureGUI::ConnectAllCameras() {
    if (!capture_system_) {
        AddLogMessage("❌ Cannot connect, capture system is not initialized.", "ERROR");
        return;
    }
    AddLogMessage("🔌 Connecting to all cameras...", "INFO");
    if (capture_system_->connectAllCameras()) {
        AddLogMessage("✅ All possible cameras connected.", "SUCCESS");
    } else {
        AddLogMessage("⚠️ Some cameras failed to connect.", "WARNING");
    }
    ShowCameraStatus();
}



void NeuralCaptureGUI::CaptureAllCameras() {
    if (!capture_system_) {
        AddLogMessage("❌ Cannot capture, capture system is not initialized.", "ERROR");
        return;
    }
    AddLogMessage("📸 Triggering capture for all connected cameras...", "INFO");
    
    if (capture_system_->captureAllCameras()) {
        std::string msg = "✅ Capture successful! Session: " + capture_system_->generateSessionName(capture_system_->getCaptureCounter() -1) ;
        AddLogMessage(msg, "SUCCESS");
    } else {
        AddLogMessage("❌ Capture failed. Check camera status and connections.", "ERROR");
    }
}

void NeuralCaptureGUI::ShowCameraStatus() {
    if (!capture_system_) return;
    AddLogMessage("--- Camera Status ---", "STATUS");
    const auto& cameras = capture_system_->getDiscoveredCameras();
    if (cameras.empty()) {
        AddLogMessage("No cameras discovered.", "STATUS");
        return;
    }
    int connected_count = 0;
    for (const auto& cam : cameras) {
        std::string status = cam.name + " (" + cam.serialNumber + "): " + (cam.isConnected ? "Connected" : "Disconnected");
        AddLogMessage(status, "STATUS");
        if(cam.isConnected) connected_count++;
    }
    AddLogMessage("Summary: " + std::to_string(connected_count) + "/" + std::to_string(cameras.size()) + " connected.", "STATUS");
    AddLogMessage("---------------------", "STATUS");
}


// Utility Methods
void NeuralCaptureGUI::AddLogMessage(const std::string& message, const std::string& level) {
    std::string timestamp = GetCurrentTimestamp();
    log_messages_.push_back("[" + timestamp + "] [" + level + "] " + message);
    if (log_messages_.size() > 500) { // Limit log size
        log_messages_.erase(log_messages_.begin());
    }
     // Also print to console for debugging
    std::cout << "[" << timestamp << "] [" << level << "] " << message << std::endl;
}

std::string NeuralCaptureGUI::GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S");
    return ss.str();
}

void NeuralCaptureGUI::OpenFolderInExplorer(const std::string& path) {
#ifdef _WIN32
    std::string command = "explorer \"" + path + "\"";
    system(command.c_str());
    AddLogMessage("Opening folder: " + path, "INFO");
#else
    AddLogMessage("OpenFolder is only implemented for Windows.", "WARNING");
#endif
}