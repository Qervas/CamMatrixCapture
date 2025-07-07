#include "NeuralCaptureSystem.hpp"
#include <iostream>

// Include the ParameterController implementation
#include "ParameterController.hpp"

NeuralRenderingCaptureSystem::NeuralRenderingCaptureSystem(const std::string& datasetPath) 
    : datasetPath_(datasetPath), currentFormat_(CaptureFormat::TIFF), captureCounter_(1), exposureTime_(40000) {
    
    // Create dataset directories
    std::filesystem::create_directories(datasetPath_ + "/images");
    std::filesystem::create_directories(datasetPath_ + "/metadata");
    
    // Initialize parameter controller
    parameterController_ = std::make_unique<ParameterController>();
    
    std::cout << "📁 Neural dataset initialized: " << datasetPath_ << std::endl;
    std::cout << "⏱️ Default exposure time: " << exposureTime_ << "μs" << std::endl;
}

NeuralRenderingCaptureSystem::~NeuralRenderingCaptureSystem() {
    // Clean up all connected cameras
    for (auto& [cameraId, camera] : connectedCameras_) {
        if (camera.transfer) {
            camera.transfer->Destroy();
            delete camera.transfer;
        }
        if (camera.buffer) {
            camera.buffer->Destroy();
            delete camera.buffer;
        }
        if (camera.acqDevice) {
            camera.acqDevice->Destroy();
            delete camera.acqDevice;
        }
    }
    connectedCameras_.clear();
    
    std::cout << "🧹 Cleanup completed" << std::endl;
}

std::vector<CameraInfo> NeuralRenderingCaptureSystem::discoverCameras() {
    std::cout << "🔍 Discovering cameras for neural rendering..." << std::endl;
    
    discoveredCameras_.clear();
    
    int serverCount = SapManager::GetServerCount();
    std::cout << "Found " << serverCount << " server(s)" << std::endl;
    
    for (int serverIndex = 0; serverIndex < serverCount; serverIndex++) {
        char serverName[CORSERVER_MAX_STRLEN];
        SapManager::GetServerName(serverIndex, serverName, sizeof(serverName));
        
        std::cout << "🖥️ Server " << (serverIndex + 1) << ": " << serverName << std::endl;
        
        int deviceCount = SapManager::GetResourceCount(serverName, SapManager::ResourceAcq);
        std::cout << "  📸 Acquisition devices: " << deviceCount << std::endl;
        
        for (int deviceIndex = 0; deviceIndex < deviceCount; deviceIndex++) {
            char deviceName[CORSERVER_MAX_STRLEN];
            SapManager::GetResourceName(serverName, SapManager::ResourceAcq, deviceIndex, deviceName, sizeof(deviceName));
            
            // Create camera info
            CameraInfo camera;
            camera.serverName = serverName;
            camera.deviceName = deviceName;
            camera.serverIndex = serverIndex;
            camera.deviceIndex = deviceIndex;
            
            // Try to get more detailed info
            SapAcqDevice* tempDevice = new SapAcqDevice(serverName, deviceName);
            if (tempDevice && tempDevice->Create()) {
                // Get serial number
                char serialBuffer[256];
                if (tempDevice->GetFeatureValue("DeviceSerialNumber", serialBuffer, sizeof(serialBuffer))) {
                    camera.serialNumber = serialBuffer;
                }
                
                // Get model name
                char modelBuffer[256];
                if (tempDevice->GetFeatureValue("DeviceModelName", modelBuffer, sizeof(modelBuffer))) {
                    camera.modelName = modelBuffer;
                }
                
                // Generate friendly name
                camera.name = "cam_" + std::to_string(discoveredCameras_.size() + 1).insert(0, 2 - std::to_string(discoveredCameras_.size() + 1).length(), '0');
                
                tempDevice->Destroy();
            }
            delete tempDevice;
            
            camera.isConnected = false;
            discoveredCameras_.push_back(camera);
            
            std::cout << "  ✅ " << camera.name << ": " << camera.serialNumber << " (" << camera.modelName << ")" << std::endl;
        }
    }
    
    std::cout << "🎯 Discovery complete: " << discoveredCameras_.size() << " cameras found" << std::endl;
    return discoveredCameras_;
}

bool NeuralRenderingCaptureSystem::connectAllCameras() {
    std::cout << "🔌 Connecting to all cameras..." << std::endl;
    
    bool allConnected = true;
    int successCount = 0;
    
    for (auto& camera : discoveredCameras_) {
        std::string cameraId = camera.serverName + "_" + camera.deviceName;
        
        if (connectCamera(cameraId)) {
            camera.isConnected = true;
            successCount++;
            std::cout << "✅ " << camera.name << " connected successfully" << std::endl;
        } else {
            camera.isConnected = false;
            allConnected = false;
            std::cout << "❌ " << camera.name << " connection failed" << std::endl;
        }
    }
    
    // Set cameras in parameter controller
    parameterController_->setCameras(connectedCameras_);
    
    std::cout << "🎯 Connection summary: " << successCount << "/" << discoveredCameras_.size() << " cameras connected" << std::endl;
    return allConnected;
}

bool NeuralRenderingCaptureSystem::connectCamera(const std::string& cameraId) {
    // Find camera info
    CameraInfo* cameraInfo = nullptr;
    for (auto& camera : discoveredCameras_) {
        if ((camera.serverName + "_" + camera.deviceName) == cameraId) {
            cameraInfo = &camera;
            break;
        }
    }
    
    if (!cameraInfo) {
        std::cerr << "❌ Camera not found: " << cameraId << std::endl;
        return false;
    }
    
    try {
        ConnectedCamera connectedCamera;
        connectedCamera.info = *cameraInfo;
        
        // Create acquisition device
        connectedCamera.acqDevice = new SapAcqDevice(cameraInfo->serverName.c_str(), cameraInfo->deviceName.c_str());
        if (!connectedCamera.acqDevice->Create()) {
            std::cerr << "❌ Failed to create acquisition device for " << cameraId << std::endl;
            delete connectedCamera.acqDevice;
            return false;
        }
        
        // Create buffer
        connectedCamera.buffer = new SapBuffer(1, connectedCamera.acqDevice);
        if (!connectedCamera.buffer->Create()) {
            std::cerr << "❌ Failed to create buffer for " << cameraId << std::endl;
            connectedCamera.acqDevice->Destroy();
            delete connectedCamera.acqDevice;
            delete connectedCamera.buffer;
            return false;
        }
        
        // Create transfer
        connectedCamera.transfer = new SapAcqDeviceToBuf(connectedCamera.acqDevice, connectedCamera.buffer);
        if (!connectedCamera.transfer->Create()) {
            std::cerr << "❌ Failed to create transfer for " << cameraId << std::endl;
            connectedCamera.buffer->Destroy();
            connectedCamera.acqDevice->Destroy();
            delete connectedCamera.acqDevice;
            delete connectedCamera.buffer;
            delete connectedCamera.transfer;
            return false;
        }
        
        // Apply exposure time
        if (!applyExposureTime(connectedCamera.acqDevice, exposureTime_)) {
            std::cerr << "⚠️ Warning: Could not set exposure time for " << cameraId << std::endl;
        }
        
        connectedCamera.connected = true;
        connectedCamera.captureReady = true;
        connectedCamera.cameraIndex = connectedCameras_.size() + 1;
        
        connectedCameras_[cameraId] = connectedCamera;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception connecting camera " << cameraId << ": " << e.what() << std::endl;
        return false;
    }
}

bool NeuralRenderingCaptureSystem::applyExposureTime(SapAcqDevice* acqDevice, int exposureTimeUs) {
    if (!acqDevice) return false;
    
    try {
        std::string exposureStr = std::to_string(exposureTimeUs);
        return acqDevice->SetFeatureValue("ExposureTime", exposureStr.c_str());
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception setting exposure time: " << e.what() << std::endl;
        return false;
    }
}

bool NeuralRenderingCaptureSystem::setExposureTime(int exposureTimeUs) {
    if (exposureTimeUs < 1000 || exposureTimeUs > 100000) {
        std::cout << "❌ Invalid exposure time. Must be between 1000-100000 μs" << std::endl;
        return false;
    }
    
    exposureTime_ = exposureTimeUs;
    
    // Apply to all connected cameras
    bool success = true;
    for (auto& [cameraId, camera] : connectedCameras_) {
        if (!applyExposureTime(camera.acqDevice, exposureTimeUs)) {
            success = false;
        }
    }
    
    if (success) {
        std::cout << "✅ Exposure time set to " << exposureTimeUs << "μs on all cameras" << std::endl;
    } else {
        std::cout << "⚠️ Exposure time partially applied. Some cameras may have failed." << std::endl;
    }
    
    return success;
}

int NeuralRenderingCaptureSystem::getExposureTime() const {
    return exposureTime_;
}

bool NeuralRenderingCaptureSystem::captureAllCameras() {
    if (connectedCameras_.empty()) {
        std::cout << "❌ No cameras connected" << std::endl;
        return false;
    }
    
    std::string sessionName = generateSessionName(captureCounter_);
    std::string sessionPath = datasetPath_ + "/images/" + sessionName;
    
    // Create session directory
    std::filesystem::create_directories(sessionPath);
    
    std::cout << "📸 Starting capture session: " << sessionName << std::endl;
    std::cout << "📁 Session path: " << sessionPath << std::endl;
    
    bool allSuccess = true;
    int successCount = 0;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Capture from all cameras
    for (auto& [cameraId, camera] : connectedCameras_) {
        if (captureSingleCamera(cameraId, sessionPath)) {
            successCount++;
        } else {
            allSuccess = false;
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    std::cout << "🎯 Capture completed in " << duration.count() << "ms" << std::endl;
    std::cout << "✅ Success: " << successCount << "/" << connectedCameras_.size() << " cameras" << std::endl;
    
    // Save session metadata
    saveSessionMetadata(sessionName, captureCounter_, allSuccess);
    
    if (allSuccess) {
        captureCounter_++;
        std::cout << "🎉 All cameras captured successfully!" << std::endl;
    } else {
        std::cout << "⚠️ Some cameras failed to capture" << std::endl;
    }
    
    return allSuccess;
}

bool NeuralRenderingCaptureSystem::captureSingleCamera(const std::string& cameraId, const std::string& sessionPath) {
    auto it = connectedCameras_.find(cameraId);
    if (it == connectedCameras_.end() || !it->second.captureReady) {
        return false;
    }
    
    ConnectedCamera& camera = it->second;
    
    try {
        // Trigger capture
        if (!camera.transfer->Snap()) {
            std::cerr << "❌ Failed to trigger capture for " << cameraId << std::endl;
            return false;
        }
        
        // Wait for capture completion
        if (!camera.transfer->Wait(5000)) {
            std::cerr << "❌ Capture timeout for " << cameraId << std::endl;
            return false;
        }
        
        // Generate filename
        std::string filename = generateImageFilename(camera.info.name, captureCounter_);
        std::string fullPath = sessionPath + "/" + filename;
        
        // Save image
        if (currentFormat_ == CaptureFormat::TIFF) {
            // Save as TIFF with color conversion
            SapColorConversion colorConv;
            if (colorConv.Create(camera.buffer, nullptr)) {
                if (colorConv.ConvertBuffer(camera.buffer, camera.buffer)) {
                    if (camera.buffer->Save(fullPath.c_str(), "-format tiff")) {
                        return true;
                    }
                }
                colorConv.Destroy();
            }
        } else {
            // Save as RAW
            if (camera.buffer->Save(fullPath.c_str(), "-format raw")) {
                return true;
            }
        }
        
        return false;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception capturing " << cameraId << ": " << e.what() << std::endl;
        return false;
    }
}

void NeuralRenderingCaptureSystem::setFormat(CaptureFormat format) {
    currentFormat_ = format;
    std::cout << "📷 Format set to: " << (format == CaptureFormat::TIFF ? "TIFF" : "RAW") << std::endl;
}

void NeuralRenderingCaptureSystem::setDatasetPath(const std::string& path) {
    datasetPath_ = path;
    std::filesystem::create_directories(datasetPath_ + "/images");
    std::filesystem::create_directories(datasetPath_ + "/metadata");
    std::cout << "📁 Dataset path set to: " << datasetPath_ << std::endl;
}

void NeuralRenderingCaptureSystem::resetCaptureCounter() {
    captureCounter_ = 1;
    std::cout << "🔄 Capture counter reset to 1" << std::endl;
}

void NeuralRenderingCaptureSystem::printCameraStatus() {
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

std::string NeuralRenderingCaptureSystem::generateSessionName(int captureNumber) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << "capture_" << std::setfill('0') << std::setw(3) << captureNumber << "_";
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    
    return ss.str();
}

std::string NeuralRenderingCaptureSystem::generateImageFilename(const std::string& cameraName, int captureNumber) {
    std::string extension = (currentFormat_ == CaptureFormat::TIFF) ? ".tiff" : ".raw";
    return cameraName + "_capture_" + std::to_string(captureNumber) + extension;
}

void NeuralRenderingCaptureSystem::saveSessionMetadata(const std::string& sessionName, int captureNumber, bool success) {
    std::string metadataPath = datasetPath_ + "/metadata/" + sessionName + ".json";
    
    std::ofstream metadataFile(metadataPath);
    if (metadataFile.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        metadataFile << "{\n";
        metadataFile << "  \"session_name\": \"" << sessionName << "\",\n";
        metadataFile << "  \"capture_number\": " << captureNumber << ",\n";
        metadataFile << "  \"timestamp\": \"" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "\",\n";
        metadataFile << "  \"format\": \"" << (currentFormat_ == CaptureFormat::TIFF ? "TIFF" : "RAW") << "\",\n";
        metadataFile << "  \"exposure_time_us\": " << exposureTime_ << ",\n";
        metadataFile << "  \"success\": " << (success ? "true" : "false") << ",\n";
        metadataFile << "  \"camera_count\": " << connectedCameras_.size() << ",\n";
        metadataFile << "  \"cameras\": [\n";
        
        bool first = true;
        for (const auto& [cameraId, camera] : connectedCameras_) {
            if (!first) metadataFile << ",\n";
            metadataFile << "    {\n";
            metadataFile << "      \"name\": \"" << camera.info.name << "\",\n";
            metadataFile << "      \"serial\": \"" << camera.info.serialNumber << "\",\n";
            metadataFile << "      \"model\": \"" << camera.info.modelName << "\"\n";
            metadataFile << "    }";
            first = false;
        }
        
        metadataFile << "\n  ]\n";
        metadataFile << "}\n";
        
        metadataFile.close();
    }
} 