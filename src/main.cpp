/**
 * main.cpp - Neural Rendering Multi-Camera Capture System
 * Supports 12 Nano-C4020 cameras for high-quality dataset generation
 */

#include "hardware/CameraTypes.hpp"
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

// Include Sapera headers directly
#include "SapClassBasic.h"

using namespace SaperaCapturePro;

enum class CaptureFormat {
    TIFF,
    RAW
};

struct CaptureSession {
    std::string sessionName;
    int captureNumber;
    CaptureFormat format;
    std::string outputPath;
    std::chrono::system_clock::time_point timestamp;
};

struct ConnectedCamera {
    CameraInfo info;
    SapAcqDevice* acqDevice = nullptr;
    SapBuffer* buffer = nullptr;
    SapAcqDeviceToBuf* transfer = nullptr;
    bool connected = false;
    bool captureReady = false;
    int cameraIndex = 0; // For neural rendering dataset naming (cam_01, cam_02, etc.)
};

class NeuralRenderingCaptureSystem {
private:
    std::vector<CameraInfo> discoveredCameras_;
    std::map<std::string, ConnectedCamera> connectedCameras_;
    std::string datasetPath_;
    CaptureFormat currentFormat_;
    int captureCounter_;
    std::string currentSessionName_;
    int exposureTime_; // Exposure time in microseconds

public:
    NeuralRenderingCaptureSystem(const std::string& datasetPath = "neural_dataset") 
        : datasetPath_(datasetPath), currentFormat_(CaptureFormat::TIFF), captureCounter_(1), exposureTime_(40000) {
        
        // Create dataset directory structure
        std::filesystem::create_directories(datasetPath_);
        std::filesystem::create_directories(datasetPath_ + "/images");
        std::filesystem::create_directories(datasetPath_ + "/metadata");
        
        std::cout << "📁 Dataset directory: " << datasetPath_ << std::endl;
        std::cout << "⏱️ Default exposure time: " << exposureTime_ << "μs" << std::endl;
    }

    std::vector<CameraInfo> discoverCameras() {
        discoveredCameras_.clear();
        
        std::cout << "🔍 Discovering cameras for neural rendering..." << std::endl;
        
        // Get server count
        int serverCount = SapManager::GetServerCount();
        std::cout << "Found " << serverCount << " server(s)" << std::endl;
        
        if (serverCount == 0) {
            std::cout << "❌ No Sapera servers found" << std::endl;
            return discoveredCameras_;
        }
        
        int cameraIndex = 1;
        
        // Enumerate all servers
        for (int serverIndex = 0; serverIndex < serverCount; serverIndex++) {
            char serverName[256];
            if (!SapManager::GetServerName(serverIndex, serverName, sizeof(serverName))) {
                std::cout << "❌ Failed to get server name for server " << serverIndex << std::endl;
                continue;
            }
            
            // Skip system server
            if (std::string(serverName) == "System") {
                continue;
            }
            
            std::cout << "🖥️ Server " << serverIndex << ": " << serverName << std::endl;
            
            // Get acquisition device count for this server
            int resourceCount = SapManager::GetResourceCount(serverName, SapManager::ResourceAcqDevice);
            std::cout << "  📸 Acquisition devices: " << resourceCount << std::endl;
            
            // Enumerate acquisition devices
            for (int resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++) {
                try {
                    // Create acquisition device temporarily for discovery
                    auto acqDevice = new SapAcqDevice(serverName, resourceIndex);
                    if (!acqDevice->Create()) {
                        std::cout << "  ❌ Failed to create device " << resourceIndex << std::endl;
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
                    camera.name = "cam_" + std::string(2 - std::to_string(cameraIndex).length(), '0') + std::to_string(cameraIndex);
                    camera.isConnected = false;
                    camera.status = CameraStatus::Disconnected;
                    camera.type = CameraType::Industrial;
                    
                    discoveredCameras_.push_back(camera);
                    std::cout << "  ✅ " << camera.name << ": " << camera.serialNumber 
                             << " (" << camera.modelName << ")" << std::endl;
                    
                    // Cleanup discovery device
                    acqDevice->Destroy();
                    delete acqDevice;
                    
                    cameraIndex++;
                    
                } catch (const std::exception& e) {
                    std::cout << "  ❌ Exception: " << e.what() << std::endl;
                }
            }
        }
        
        std::cout << "✅ Discovery complete: " << discoveredCameras_.size() << " cameras found" << std::endl;
        return discoveredCameras_;
    }
    
    bool connectAllCameras() {
        std::cout << "🔗 Connecting to all cameras..." << std::endl;
        
        int successCount = 0;
        int totalCameras = discoveredCameras_.size();
        
        for (const auto& cameraInfo : discoveredCameras_) {
            if (connectCamera(cameraInfo.id)) {
                successCount++;
                std::cout << "  ✅ " << cameraInfo.name << " connected" << std::endl;
            } else {
                std::cout << "  ❌ " << cameraInfo.name << " failed to connect" << std::endl;
            }
        }
        
        std::cout << "🎯 Connected " << successCount << "/" << totalCameras << " cameras" << std::endl;
        return successCount == totalCameras;
    }
    
    bool connectCamera(const std::string& cameraId) {
        // Check if already connected
        if (connectedCameras_.find(cameraId) != connectedCameras_.end()) {
            return true;
        }
        
        // Find camera info
        CameraInfo* cameraInfo = nullptr;
        for (auto& cam : discoveredCameras_) {
            if (cam.id == cameraId) {
                cameraInfo = &cam;
                break;
            }
        }
        
        if (!cameraInfo) {
            return false;
        }
        
        try {
            // Create acquisition device for this camera
            auto acqDevice = new SapAcqDevice(cameraInfo->serverName.c_str(), cameraInfo->resourceIndex);
            if (!acqDevice->Create()) {
                delete acqDevice;
                return false;
            }
            
            // Apply exposure time setting
            if (!applyExposureTime(acqDevice, exposureTime_)) {
                std::cout << "⚠️ Warning: Failed to set exposure time for " << cameraInfo->name << std::endl;
            }
            
            // Create buffer for image capture
            auto buffer = new SapBufferWithTrash(1, acqDevice);
            if (!buffer->Create()) {
                acqDevice->Destroy();
                delete acqDevice;
                delete buffer;
                return false;
            }
            
            // Create transfer object
            auto transfer = new SapAcqDeviceToBuf(acqDevice, buffer);
            if (!transfer->Create()) {
                buffer->Destroy();
                acqDevice->Destroy();
                delete transfer;
                delete buffer;
                delete acqDevice;
                return false;
            }
            
            // Store connected camera with capture objects
            ConnectedCamera connectedCam;
            connectedCam.info = *cameraInfo;
            connectedCam.info.isConnected = true;
            connectedCam.info.status = CameraStatus::Ready;
            connectedCam.acqDevice = acqDevice;
            connectedCam.buffer = buffer;
            connectedCam.transfer = transfer;
            connectedCam.connected = true;
            connectedCam.captureReady = true;
            connectedCam.cameraIndex = std::stoi(cameraId);
            
            connectedCameras_[cameraId] = connectedCam;
            
            // Update the discovered camera status
            cameraInfo->isConnected = true;
            cameraInfo->status = CameraStatus::Ready;
            
            return true;
            
        } catch (const std::exception& e) {
            return false;
        }
    }
    
    // Apply exposure time to a camera
    bool applyExposureTime(SapAcqDevice* acqDevice, int exposureTimeUs) {
        if (!acqDevice) {
            return false;
        }
        
        try {
            // Set exposure time in microseconds
            std::string exposureStr = std::to_string(exposureTimeUs);
            if (!acqDevice->SetFeatureValue("ExposureTime", exposureStr.c_str())) {
                // Try alternative parameter name
                if (!acqDevice->SetFeatureValue("ExposureTimeAbs", exposureStr.c_str())) {
                    return false;
                }
            }
            
            // Wait for parameter to take effect
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            return true;
            
        } catch (const std::exception& e) {
            return false;
        }
    }
    
    // Set exposure time for all connected cameras
    bool setExposureTime(int exposureTimeUs) {
        if (exposureTimeUs < 1000 || exposureTimeUs > 100000) {
            std::cout << "❌ Invalid exposure time. Must be between 1000-100000 μs" << std::endl;
            return false;
        }
        
        exposureTime_ = exposureTimeUs;
        std::cout << "⏱️ Setting exposure time to " << exposureTime_ << "μs for all cameras..." << std::endl;
        
        int successCount = 0;
        for (auto& [cameraId, cam] : connectedCameras_) {
            if (applyExposureTime(cam.acqDevice, exposureTime_)) {
                successCount++;
                std::cout << "  ✅ " << cam.info.name << ": " << exposureTime_ << "μs" << std::endl;
            } else {
                std::cout << "  ❌ " << cam.info.name << ": Failed to set exposure" << std::endl;
            }
        }
        
        std::cout << "🎯 Updated " << successCount << "/" << connectedCameras_.size() << " cameras" << std::endl;
        return successCount == connectedCameras_.size();
    }
    
    // Get current exposure time
    int getExposureTime() const {
        return exposureTime_;
    }
    
    bool captureAllCameras() {
        if (connectedCameras_.empty()) {
            std::cout << "❌ No cameras connected" << std::endl;
            return false;
        }
        
        currentSessionName_ = generateSessionName(captureCounter_);
        
        std::cout << "📸 Starting capture session #" << captureCounter_ << std::endl;
        std::cout << "🎬 Session: " << currentSessionName_ << std::endl;
        
        // Create session directory
        std::string sessionPath = datasetPath_ + "/images/" + currentSessionName_;
        std::filesystem::create_directories(sessionPath);
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Capture all cameras simultaneously
        std::vector<std::thread> captureThreads;
        std::vector<bool> captureResults(connectedCameras_.size(), false);
        
        int threadIndex = 0;
        for (auto& [cameraId, cam] : connectedCameras_) {
            captureThreads.emplace_back([&, threadIndex, cameraId]() {
                captureResults[threadIndex] = captureSingleCamera(cameraId, sessionPath);
            });
            threadIndex++;
        }
        
        // Wait for all captures to complete
        for (auto& thread : captureThreads) {
            thread.join();
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        // Check results
        int successCount = 0;
        for (bool result : captureResults) {
            if (result) successCount++;
        }
        
        std::cout << "🎯 Capture complete: " << successCount << "/" << connectedCameras_.size() 
                  << " cameras (" << duration.count() << "ms)" << std::endl;
        
        // Save metadata
        saveSessionMetadata(currentSessionName_, captureCounter_, successCount == connectedCameras_.size());
        
        if (successCount == connectedCameras_.size()) {
            captureCounter_++;
            std::cout << "✅ All cameras captured successfully! Ready for next capture." << std::endl;
        }
        
        return successCount == connectedCameras_.size();
    }
    
    bool captureSingleCamera(const std::string& cameraId, const std::string& sessionPath) {
        auto it = connectedCameras_.find(cameraId);
        if (it == connectedCameras_.end() || !it->second.captureReady) {
            return false;
        }
        
        ConnectedCamera& cam = it->second;
        
        try {
            // 1. Snap (trigger single capture) - NOT Grab!
            if (!cam.transfer->Snap()) {
                std::cerr << "❌ Snap failed for " << cam.info.name << std::endl;
                return false;
            }
            
            // 2. Wait for capture to complete - CRITICAL!
            if (!cam.transfer->Wait(10000)) {  // 10 second timeout
                std::cerr << "❌ Wait timeout for " << cam.info.name << std::endl;
                cam.transfer->Abort();
                return false;
            }
            
            // 3. Create fresh color converter for this capture
            SapColorConversion colorConverter(cam.buffer);
            if (!colorConverter.Create()) {
                std::cerr << "❌ Failed to create color converter for " << cam.info.name << std::endl;
                return false;
            }
            
            // 4. Configure converter
            colorConverter.Enable(TRUE, FALSE);
            colorConverter.SetOutputFormat(SapFormatRGB888);
            colorConverter.SetAlign(SapColorConversion::AlignRGGB);
            
            // 5. Convert the image
            if (!colorConverter.Convert()) {
                std::cerr << "❌ Color conversion failed for " << cam.info.name << std::endl;
                colorConverter.Destroy();
                return false;
            }
            
            // 6. Get output buffer
            SapBuffer* outputBuffer = colorConverter.GetOutputBuffer();
            if (!outputBuffer) {
                std::cerr << "❌ No output buffer for " << cam.info.name << std::endl;
                colorConverter.Destroy();
                return false;
            }
            
            // 7. Generate filename and save
            std::string filename = generateImageFilename(cam.info.name, captureCounter_);
            std::string fullPath = sessionPath + "/" + filename;
            
            bool saveSuccess = false;
            if (currentFormat_ == CaptureFormat::TIFF) {
                saveSuccess = outputBuffer->Save(fullPath.c_str(), "-format tiff");
            } else {
                // Save as RAW Sapera format
                std::string rawPath = fullPath;
                rawPath.replace(rawPath.find(".tiff"), 5, ".raw");
                saveSuccess = outputBuffer->Save(rawPath.c_str(), "-format raw");
            }
            
            // 8. Clean up converter
            colorConverter.Destroy();
            
            if (!saveSuccess) {
                std::cerr << "❌ Failed to save " << fullPath << std::endl;
                return false;
            }
            
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Exception in " << cam.info.name << ": " << e.what() << std::endl;
            return false;
        }
    }
    
    void setFormat(CaptureFormat format) {
        currentFormat_ = format;
        std::cout << "📷 Format set to: " << (format == CaptureFormat::TIFF ? "TIFF" : "RAW") << std::endl;
    }
    
    void resetCaptureCounter() {
        captureCounter_ = 1;
        std::cout << "🔄 Capture counter reset to 1" << std::endl;
    }
    
    void printCameraStatus() {
        std::cout << "\n=== Multi-Camera Neural Rendering System Status ===" << std::endl;
        std::cout << "📁 Dataset: " << datasetPath_ << std::endl;
        std::cout << "📷 Format: " << (currentFormat_ == CaptureFormat::TIFF ? "TIFF" : "RAW") << std::endl;
        std::cout << "⏱️ Exposure: " << exposureTime_ << "μs" << std::endl;
        std::cout << "🎯 Cameras: " << connectedCameras_.size() << "/" << discoveredCameras_.size() << " connected" << std::endl;
        std::cout << "📸 Next capture: #" << captureCounter_ << std::endl;
        
        if (discoveredCameras_.empty()) {
            std::cout << "No cameras discovered" << std::endl;
            return;
        }
        
        for (const auto& camera : discoveredCameras_) {
            std::cout << "📸 " << camera.name << " (" << camera.serialNumber << "): " 
                      << (camera.isConnected ? "🟢 Ready" : "🔴 Disconnected") << std::endl;
        }
    }
    
    void runInteractiveSession() {
        std::cout << "\n🎬 Starting Interactive Neural Rendering Capture Session" << std::endl;
        std::cout << "Commands:" << std::endl;
        std::cout << "  capture              - Capture all cameras (current position)" << std::endl;
        std::cout << "  format tiff|raw      - Switch capture format" << std::endl;
        std::cout << "  exposure <time>      - Set exposure time in microseconds (1000-100000)" << std::endl;
        std::cout << "  reset                - Reset capture counter to 1" << std::endl;
        std::cout << "  status               - Show system status" << std::endl;
        std::cout << "  help                 - Show this help" << std::endl;
        std::cout << "  quit                 - Exit application" << std::endl;
        std::cout << std::endl;
        std::cout << "💡 Workflow: Manually position object → type 'capture' → repeat" << std::endl;
        std::cout << std::endl;
        
        std::string command;
        while (true) {
            std::cout << "neural_capture> ";
            std::getline(std::cin, command);
            
            if (command.empty()) continue;
            
            std::istringstream iss(command);
            std::string cmd;
            iss >> cmd;
            
            if (cmd == "quit" || cmd == "exit") {
                break;
            } else if (cmd == "capture") {
                std::cout << "📸 Capturing all cameras at current object position..." << std::endl;
                captureAllCameras();
            } else if (cmd == "format") {
                std::string format;
                if (iss >> format) {
                    if (format == "tiff") {
                        setFormat(CaptureFormat::TIFF);
                    } else if (format == "raw") {
                        setFormat(CaptureFormat::RAW);
                    } else {
                        std::cout << "❌ Invalid format. Use 'tiff' or 'raw'" << std::endl;
                    }
                } else {
                    std::cout << "❌ Usage: format <tiff|raw>" << std::endl;
                }
            } else if (cmd == "exposure") {
                int newExposure;
                if (iss >> newExposure) {
                    setExposureTime(newExposure);
                } else {
                    std::cout << "⏱️ Current exposure time: " << getExposureTime() << "μs" << std::endl;
                    std::cout << "❌ Usage: exposure <time_in_microseconds>" << std::endl;
                    std::cout << "   Example: exposure 40000" << std::endl;
                }
            } else if (cmd == "reset") {
                resetCaptureCounter();
            } else if (cmd == "status") {
                printCameraStatus();
            } else if (cmd == "help") {
                std::cout << "Available commands:" << std::endl;
                std::cout << "  capture              - Capture all cameras (current position)" << std::endl;
                std::cout << "  format tiff|raw      - Switch capture format" << std::endl;
                std::cout << "  exposure <time>      - Set exposure time in microseconds (1000-100000)" << std::endl;
                std::cout << "  reset                - Reset capture counter to 1" << std::endl;
                std::cout << "  status               - Show system status" << std::endl;
                std::cout << "  help                 - Show this help" << std::endl;
                std::cout << "  quit                 - Exit application" << std::endl;
                std::cout << std::endl;
                std::cout << "💡 Workflow: Manually position object → type 'capture' → repeat" << std::endl;
                std::cout << "⏱️ Default exposure: 40000μs (40ms)" << std::endl;
            } else {
                std::cout << "❌ Unknown command. Type 'help' for available commands." << std::endl;
            }
        }
    }
    
    ~NeuralRenderingCaptureSystem() {
        // Clean up all connected cameras
        for (auto& [cameraId, cam] : connectedCameras_) {
            if (cam.transfer) {
                cam.transfer->Destroy();
                delete cam.transfer;
            }
            if (cam.buffer) {
                cam.buffer->Destroy();
                delete cam.buffer;
            }
            if (cam.acqDevice) {
                cam.acqDevice->Destroy();
                delete cam.acqDevice;
            }
        }
        connectedCameras_.clear();
    }

private:
    std::string generateSessionName(int captureNumber) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::ostringstream oss;
        oss << "capture_" << std::setfill('0') << std::setw(3) << captureNumber 
            << "_" << std::put_time(&tm, "%Y%m%d_%H%M%S");
        return oss.str();
    }
    
    std::string generateImageFilename(const std::string& cameraName, int captureNumber) {
        std::ostringstream oss;
        oss << cameraName << "_capture_" << std::setfill('0') << std::setw(3) << captureNumber;
        if (currentFormat_ == CaptureFormat::TIFF) {
            oss << ".tiff";
        } else {
            oss << ".raw";
        }
        return oss.str();
    }
    
    void saveSessionMetadata(const std::string& sessionName, int captureNumber, bool success) {
        std::string metadataPath = datasetPath_ + "/metadata/" + sessionName + ".json";
        
        // Create simple metadata file
        std::ofstream metadataFile(metadataPath);
        if (metadataFile.is_open()) {
            metadataFile << "{\n";
            metadataFile << "  \"session_name\": \"" << sessionName << "\",\n";
            metadataFile << "  \"capture_number\": " << captureNumber << ",\n";
            metadataFile << "  \"capture_success\": " << (success ? "true" : "false") << ",\n";
            metadataFile << "  \"camera_count\": " << connectedCameras_.size() << ",\n";
            metadataFile << "  \"format\": \"" << (currentFormat_ == CaptureFormat::TIFF ? "TIFF" : "RAW") << "\",\n";
            metadataFile << "  \"timestamp\": \"" << std::time(nullptr) << "\",\n";
            metadataFile << "  \"cameras\": [\n";
            
            bool first = true;
            for (const auto& [cameraId, cam] : connectedCameras_) {
                if (!first) metadataFile << ",\n";
                metadataFile << "    {\n";
                metadataFile << "      \"id\": \"" << cameraId << "\",\n";
                metadataFile << "      \"name\": \"" << cam.info.name << "\",\n";
                metadataFile << "      \"serial\": \"" << cam.info.serialNumber << "\",\n";
                metadataFile << "      \"server\": \"" << cam.info.serverName << "\"\n";
                metadataFile << "    }";
                first = false;
            }
            
            metadataFile << "\n  ]\n";
            metadataFile << "}\n";
            metadataFile.close();
        }
    }
};

void printBanner() {
    std::cout << R"(
🎬 ================================================= 🎬
   Neural Rendering Multi-Camera Capture System
   12 x Nano-C4020 Cameras • 4112x3008 Resolution
   Manual Rotation Workflow
🎬 ================================================= 🎬
)" << std::endl;
}

int main() {
    printBanner();
    
    try {
        NeuralRenderingCaptureSystem captureSystem("neural_dataset");
        
        // 1. Discover cameras
        std::cout << "Step 1: Camera Discovery" << std::endl;
        auto cameras = captureSystem.discoverCameras();
        
        if (cameras.empty()) {
            std::cout << "❌ No cameras found. Check your Sapera installation and camera connections." << std::endl;
            return 1;
        }
        
        // 2. Connect to all cameras
        std::cout << "\nStep 2: Multi-Camera Connection" << std::endl;
        if (!captureSystem.connectAllCameras()) {
            std::cout << "⚠️ Some cameras failed to connect. Continuing with available cameras..." << std::endl;
        }
        
        // 3. Show status
        captureSystem.printCameraStatus();
        
        // 4. Start interactive session
        std::cout << "\nStep 3: Interactive Capture Session" << std::endl;
        captureSystem.runInteractiveSession();
        
        std::cout << "\n🎉 Neural rendering capture session completed!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Unhandled exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "❌ Unknown error occurred" << std::endl;
        return 1;
    }
    
    return 0;
} 