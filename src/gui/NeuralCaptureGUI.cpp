#include "NeuralCaptureGUI.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>

NeuralCaptureGUI::NeuralCaptureGUI() {
    current_image_folder_ = "neural_dataset";
    std::strcpy(image_folder_buffer_, current_image_folder_.c_str());
    
    // Initialize REAL capture system
    try {
        capture_system_ = std::make_unique<NeuralRenderingCaptureSystem>(current_image_folder_);
        AddLogMessage("✅ REAL NeuralRenderingCaptureSystem initialized");
    } catch (const std::exception& e) {
        AddLogMessage("❌ Failed to initialize capture system: " + std::string(e.what()));
        capture_system_ = nullptr;
    }
    
    AddLogMessage("🎬 Neural Capture GUI initialized");
}

NeuralCaptureGUI::~NeuralCaptureGUI() {
    Shutdown();
}

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
    window_ = glfwCreateWindow(1800, 1000, "Neural Rendering Capture System - INTEGRATED", NULL, NULL);
    if (window_ == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
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
    ImGui::StyleColorsDark();
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    AddLogMessage("✅ GUI initialized successfully");
    return true;
}

void NeuralCaptureGUI::Run() {
    AddLogMessage("🚀 Starting REAL integrated GUI");
    
    while (running_ && !glfwWindowShouldClose(window_)) {
        glfwPollEvents();
        
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Render GUI
        RenderMainMenuBar();
        RenderPanels();
        
        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.15f, 0.15f, 0.15f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window_);
        
        // Small delay to prevent excessive CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    AddLogMessage("🛑 Application shutting down");
}

void NeuralCaptureGUI::Shutdown() {
    if (capture_system_) {
        AddLogMessage("🔌 Shutting down capture system");
        capture_system_.reset();
    }
    
    if (window_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        glfwDestroyWindow(window_);
        glfwTerminate();
        window_ = nullptr;
    }
}

void NeuralCaptureGUI::GlfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

void NeuralCaptureGUI::RenderMainMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("System")) {
            if (ImGui::MenuItem("Discover Cameras")) {
                DiscoverCameras();
            }
            if (ImGui::MenuItem("Connect All Cameras")) {
                ConnectAllCameras();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                running_ = false;
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
        
        // Status
        ImGui::Text("Status:");
        if (capture_system_) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "REAL System Ready");
        } else {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "System Error");
        }
        
        ImGui::EndMainMenuBar();
    }
}

void NeuralCaptureGUI::RenderPanels() {
    if (show_camera_panel_) {
        RenderCameraPanel();
    }
    
    if (show_parameter_panel_) {
        RenderParameterPanel();
    }
    
    if (show_capture_panel_) {
        RenderCapturePanel();
    }
    
    if (show_log_panel_) {
        RenderLogPanel();
    }
}

void NeuralCaptureGUI::RenderCameraPanel() {
    ImGui::Begin("Camera System", &show_camera_panel_);
    
    ImGui::Text("REAL Sapera SDK Camera Control");
    ImGui::Separator();
    
    // Control buttons
    if (ImGui::Button("🔍 Discover Cameras", ImVec2(200, 40))) {
        DiscoverCameras();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("🔌 Connect All", ImVec2(200, 40))) {
        ConnectAllCameras();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("📊 Status", ImVec2(200, 40))) {
        ShowCameraStatus();
    }
    
    ImGui::Separator();
    
    // Camera table
    if (capture_system_) {
        const auto& discovered = capture_system_->getDiscoveredCameras();
        const auto& connected = capture_system_->getConnectedCameras();
        
        if (ImGui::BeginTable("CameraTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Camera");
            ImGui::TableSetupColumn("Serial Number");
            ImGui::TableSetupColumn("Model");
            ImGui::TableSetupColumn("Status");
            ImGui::TableSetupColumn("Actions");
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
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "🔴 Disconnected");
                }
                
                ImGui::TableSetColumnIndex(4);
                std::string button_id = "Info##" + camera.serialNumber;
                if (ImGui::Button(button_id.c_str(), ImVec2(60, 20))) {
                    AddLogMessage("📸 Camera: " + camera.name + " (" + camera.serialNumber + ")");
                }
            }
            
            ImGui::EndTable();
        }
        
        ImGui::Text("Connected: %d / %d cameras", (int)connected.size(), (int)discovered.size());
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "❌ Capture system not initialized");
    }
    
    ImGui::End();
}

void NeuralCaptureGUI::RenderParameterPanel() {
    ImGui::Begin("Parameters", &show_parameter_panel_);
    
    ImGui::Text("REAL Camera Parameters");
    ImGui::Separator();
    
    if (capture_system_) {
        // Exposure time
        int exposure = capture_system_->getExposureTime();
        if (ImGui::SliderInt("Exposure Time (μs)", &exposure, 1000, 100000)) {
            capture_system_->setExposureTime(exposure);
            AddLogMessage("⚙️ Exposure time set to " + std::to_string(exposure) + "μs");
        }
        
        ImGui::Separator();
        
        // Format selection
        ImGui::Text("Capture Format:");
        const char* formats[] = { "TIFF", "RAW" };
        int current_format = (capture_system_->getCurrentFormat() == CaptureFormat::TIFF) ? 0 : 1;
        if (ImGui::Combo("##format", &current_format, formats, 2)) {
            CaptureFormat format = (current_format == 0) ? CaptureFormat::TIFF : CaptureFormat::RAW;
            capture_system_->setFormat(format);
            AddLogMessage("📷 Format set to " + std::string(formats[current_format]));
        }
        
        ImGui::Separator();
        
        // Quick actions
        if (ImGui::Button("📋 List All Parameters", ImVec2(200, 30))) {
            // This would need parameter controller access
            AddLogMessage("📋 Parameter list requested");
        }
        
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "❌ Capture system not available");
    }
    
    ImGui::End();
}

void NeuralCaptureGUI::RenderCapturePanel() {
    ImGui::Begin("Capture Control", &show_capture_panel_);
    
    ImGui::Text("REAL Neural Rendering Capture");
    ImGui::Separator();
    
    if (capture_system_) {
        // Session info
        ImGui::Text("Dataset: %s", capture_system_->getDatasetPath().c_str());
        ImGui::Text("Next Capture: #%d", capture_system_->getCaptureCounter());
        
        // Folder management
        ImGui::Text("Output Folder:");
        ImGui::SetNextItemWidth(400);
        if (ImGui::InputText("##ImageFolder", image_folder_buffer_, sizeof(image_folder_buffer_))) {
            current_image_folder_ = image_folder_buffer_;
            capture_system_->setDatasetPath(current_image_folder_);
            AddLogMessage("📁 Dataset path changed to: " + current_image_folder_);
        }
        
        ImGui::SameLine();
        if (ImGui::Button("📂 Open")) {
            OpenFolderInExplorer(current_image_folder_);
        }
        
        ImGui::SameLine();
        if (ImGui::Button("📁 Create")) {
            try {
                std::filesystem::create_directories(current_image_folder_);
                AddLogMessage("📁 Created folder: " + current_image_folder_);
            } catch (const std::exception& e) {
                AddLogMessage("❌ Error creating folder: " + std::string(e.what()));
            }
        }
        
        // Folder status
        if (std::filesystem::exists(current_image_folder_)) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✅ Folder exists");
            
            // Count files
            try {
                int file_count = 0;
                std::string images_path = current_image_folder_ + "/images";
                if (std::filesystem::exists(images_path)) {
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(images_path)) {
                        if (entry.is_regular_file()) {
                            file_count++;
                        }
                    }
                }
                ImGui::SameLine();
                ImGui::Text("(%d files)", file_count);
            } catch (...) {
                ImGui::SameLine();
                ImGui::Text("(Cannot count files)");
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "⚠️ Folder will be created");
        }
        
        ImGui::Separator();
        
        // MAIN CAPTURE BUTTON
        if (ImGui::Button("🎬 CAPTURE ALL CAMERAS", ImVec2(300, 60))) {
            CaptureAllCameras();
        }
        
        ImGui::SameLine();
        if (ImGui::Button("🔄 Reset Counter", ImVec2(150, 60))) {
            capture_system_->resetCaptureCounter();
            AddLogMessage("🔄 Capture counter reset");
        }
        
        ImGui::Separator();
        
        // File size info
        const auto& connected = capture_system_->getConnectedCameras();
        if (!connected.empty()) {
            ImGui::Text("Estimated file sizes:");
            if (capture_system_->getCurrentFormat() == CaptureFormat::TIFF) {
                ImGui::BulletText("Per camera: ~37 MB (TIFF)");
                ImGui::BulletText("Total (%d cameras): ~%d MB per capture", 
                    (int)connected.size(), (int)connected.size() * 37);
            } else {
                ImGui::BulletText("Per camera: ~12 MB (RAW)");
                ImGui::BulletText("Total (%d cameras): ~%d MB per capture", 
                    (int)connected.size(), (int)connected.size() * 12);
            }
        }
        
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "❌ Capture system not available");
    }
    
    ImGui::End();
}

void NeuralCaptureGUI::RenderLogPanel() {
    ImGui::Begin("System Log", &show_log_panel_);
    
    ImGui::Text("REAL System Messages");
    ImGui::Separator();
    
    // Log controls
    if (ImGui::Button("Clear Log")) {
        log_messages_.clear();
    }
    
    ImGui::SameLine();
    ImGui::Text("(%d messages)", (int)log_messages_.size());
    
    ImGui::Separator();
    
    // Log messages
    ImGui::BeginChild("LogMessages", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    
    for (const auto& message : log_messages_) {
        // Color code messages
        if (message.find("✅") != std::string::npos) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", message.c_str());
        } else if (message.find("❌") != std::string::npos) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", message.c_str());
        } else if (message.find("⚠️") != std::string::npos) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "%s", message.c_str());
        } else if (message.find("🎬") != std::string::npos) {
            ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "%s", message.c_str());
        } else {
            ImGui::Text("%s", message.c_str());
        }
    }
    
    // Auto-scroll
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    
    ImGui::EndChild();
    
    ImGui::End();
}

void NeuralCaptureGUI::DiscoverCameras() {
    if (!capture_system_) {
        AddLogMessage("❌ Capture system not available");
        return;
    }
    
    AddLogMessage("🔍 Discovering REAL cameras...");
    
    try {
        auto cameras = capture_system_->discoverCameras();
        AddLogMessage("✅ Discovered " + std::to_string(cameras.size()) + " REAL cameras");
        
        for (const auto& camera : cameras) {
            AddLogMessage("📷 " + camera.name + " (" + camera.serialNumber + ")");
        }
        
    } catch (const std::exception& e) {
        AddLogMessage("❌ Error discovering cameras: " + std::string(e.what()));
    }
}

void NeuralCaptureGUI::ConnectAllCameras() {
    if (!capture_system_) {
        AddLogMessage("❌ Capture system not available");
        return;
    }
    
    AddLogMessage("🔌 Connecting to REAL cameras...");
    
    try {
        bool success = capture_system_->connectAllCameras();
        
        if (success) {
            AddLogMessage("🎉 All cameras connected successfully!");
        } else {
            AddLogMessage("⚠️ Some cameras failed to connect");
        }
        
    } catch (const std::exception& e) {
        AddLogMessage("❌ Error connecting cameras: " + std::string(e.what()));
    }
}

void NeuralCaptureGUI::CaptureAllCameras() {
    if (!capture_system_) {
        AddLogMessage("❌ Capture system not available");
        return;
    }
    
    AddLogMessage("🎬 REAL CAPTURE starting...");
    
    try {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        bool success = capture_system_->captureAllCameras();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (success) {
            AddLogMessage("🎉 REAL CAPTURE completed in " + std::to_string(duration.count()) + "ms");
            AddLogMessage("📁 Files saved to: " + current_image_folder_);
            
            // Count created files
            try {
                int file_count = 0;
                std::string images_path = current_image_folder_ + "/images";
                if (std::filesystem::exists(images_path)) {
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(images_path)) {
                        if (entry.is_regular_file()) {
                            file_count++;
                        }
                    }
                }
                AddLogMessage("✅ Total files in dataset: " + std::to_string(file_count));
            } catch (const std::exception& e) {
                AddLogMessage("⚠️ Could not count files: " + std::string(e.what()));
            }
            
        } else {
            AddLogMessage("❌ REAL CAPTURE failed");
        }
        
    } catch (const std::exception& e) {
        AddLogMessage("❌ Error during capture: " + std::string(e.what()));
    }
}

void NeuralCaptureGUI::ShowCameraStatus() {
    if (!capture_system_) {
        AddLogMessage("❌ Capture system not available");
        return;
    }
    
    AddLogMessage("📊 Showing camera status...");
    
    try {
        capture_system_->printCameraStatus();
        AddLogMessage("✅ Camera status displayed in console");
        
    } catch (const std::exception& e) {
        AddLogMessage("❌ Error showing camera status: " + std::string(e.what()));
    }
}

void NeuralCaptureGUI::AddLogMessage(const std::string& message) {
    std::string timestamp = GetCurrentTimestamp();
    std::string formatted_message = "[" + timestamp + "] " + message;
    
    log_messages_.push_back(formatted_message);
    
    // Keep only last 200 messages
    if (log_messages_.size() > 200) {
        log_messages_.erase(log_messages_.begin());
    }
    
    // Also print to console
    std::cout << formatted_message << std::endl;
}

std::string NeuralCaptureGUI::GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    
    return ss.str();
}

void NeuralCaptureGUI::OpenFolderInExplorer(const std::string& path) {
    if (std::filesystem::exists(path)) {
#ifdef _WIN32
        std::string command = "explorer \"" + path + "\"";
        system(command.c_str());
#endif
        AddLogMessage("📂 Opened folder: " + path);
    } else {
        AddLogMessage("❌ Folder does not exist: " + path);
    }
} 