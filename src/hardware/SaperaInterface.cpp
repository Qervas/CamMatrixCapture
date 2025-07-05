/**
 * SaperaInterface.cpp - Working Direct Sapera SDK Integration
 * Based on proven simple_interface.cpp approach
 */

#include "SaperaInterface.hpp"
#include <iostream>
#include <map>

// Include Sapera headers directly
#include "sapclassbasic.h"

namespace SaperaCapturePro {

struct ConnectedCamera {
    CameraInfo info;
    SapAcqDevice* acqDevice = nullptr;
    SapBuffer* buffer = nullptr;
    SapAcqDeviceToBuf* transfer = nullptr;
    bool connected = false;
    bool captureReady = false;
};

class SaperaInterface::Impl {
private:
    std::vector<CameraInfo> discoveredCameras_;
    std::map<std::string, ConnectedCamera> connectedCameras_;
    bool initialized_ = false;

public:
    bool initialize() {
        std::cout << "Initializing SaperaInterface..." << std::endl;
        
        // Simple initialization - just discover cameras
        discoveredCameras_ = discoverCameras();
        initialized_ = !discoveredCameras_.empty();
        
        if (initialized_) {
            std::cout << "SaperaInterface initialized with " << discoveredCameras_.size() << " cameras" << std::endl;
        } else {
            std::cout << "SaperaInterface initialization failed - no cameras found" << std::endl;
        }
        
        return initialized_;
    }
    
    std::vector<CameraInfo> discoverCameras() {
        discoveredCameras_.clear();
        
        std::cout << "Discovering cameras..." << std::endl;
        
        // Get server count
        int serverCount = SapManager::GetServerCount();
        std::cout << "Found " << serverCount << " server(s)" << std::endl;
        
        if (serverCount == 0) {
            std::cout << "No Sapera servers found" << std::endl;
            return discoveredCameras_;
        }
        
        // Enumerate all servers
        for (int serverIndex = 0; serverIndex < serverCount; serverIndex++) {
            char serverName[256];
            if (!SapManager::GetServerName(serverIndex, serverName, sizeof(serverName))) {
                std::cout << "Failed to get server name" << std::endl;
                continue;
            }
            
            std::cout << "Server " << serverIndex << ": " << serverName << std::endl;
            
            // Get acquisition device count for this server
            int resourceCount = SapManager::GetResourceCount(serverName, SapManager::ResourceAcqDevice);
            std::cout << "  Acquisition devices: " << resourceCount << std::endl;
            
            // Enumerate acquisition devices
            for (int resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++) {
                try {
                    // Create acquisition device temporarily for discovery
                    auto acqDevice = new SapAcqDevice(serverName, resourceIndex);
                    if (!acqDevice->Create()) {
                        std::cout << "  Failed to create device " << resourceIndex << std::endl;
                        delete acqDevice;
                        continue;
                    }
                    
                    // Get device information
                    char buffer[512];
                    
                    CameraInfo camera;
                    camera.id = std::to_string(discoveredCameras_.size() + 1);
                    camera.serverName = serverName;
                    camera.resourceIndex = resourceIndex;
                    
                    // Get serial number
                    if (acqDevice->GetFeatureValue("DeviceSerialNumber", buffer, sizeof(buffer))) {
                        camera.serialNumber = std::string(buffer);
                    } else {
                        camera.serialNumber = "Unknown_" + std::to_string(resourceIndex);
                    }
                    
                    // Get model name
                    if (acqDevice->GetFeatureValue("DeviceModelName", buffer, sizeof(buffer))) {
                        camera.modelName = std::string(buffer);
                    } else {
                        camera.modelName = "Unknown_Model";
                    }
                    
                    camera.name = camera.modelName + "_" + camera.id;
                    camera.isConnected = false;
                    camera.status = CameraStatus::Disconnected;
                    camera.type = CameraType::Industrial;
                    
                    discoveredCameras_.push_back(camera);
                    std::cout << "  ✅ Camera " << resourceIndex << ": " << camera.serialNumber 
                             << " (" << camera.modelName << ")" << std::endl;
                    
                    // Cleanup discovery device
                    acqDevice->Destroy();
                    delete acqDevice;
                    
                } catch (const std::exception& e) {
                    std::cout << "  Exception: " << e.what() << std::endl;
                }
            }
        }
        
        std::cout << "Discovery complete: " << discoveredCameras_.size() << " cameras found" << std::endl;
        return discoveredCameras_;
    }
    
    bool connectCamera(const std::string& cameraId) {
        std::cout << "Connecting to camera: " << cameraId << std::endl;
        
        // Check if already connected
        if (connectedCameras_.find(cameraId) != connectedCameras_.end()) {
            std::cout << "Camera " << cameraId << " already connected" << std::endl;
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
            std::cout << "Camera " << cameraId << " not found in discovered cameras" << std::endl;
            return false;
        }
        
        try {
            // Create acquisition device for this camera
            auto acqDevice = new SapAcqDevice(cameraInfo->serverName.c_str(), cameraInfo->resourceIndex);
            if (!acqDevice->Create()) {
                std::cout << "Failed to create acquisition device for camera " << cameraId << std::endl;
                delete acqDevice;
                return false;
            }
            
            // Create buffer for image capture
            auto buffer = new SapBufferWithTrash(1, acqDevice);
            if (!buffer->Create()) {
                std::cout << "Failed to create buffer for camera " << cameraId << std::endl;
                acqDevice->Destroy();
                delete acqDevice;
                delete buffer;
                return false;
            }
            
            // Create transfer object
            auto transfer = new SapAcqDeviceToBuf(acqDevice, buffer);
            if (!transfer->Create()) {
                std::cout << "Failed to create transfer for camera " << cameraId << std::endl;
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
            
            connectedCameras_[cameraId] = connectedCam;
            
            // Update the discovered camera status
            cameraInfo->isConnected = true;
            cameraInfo->status = CameraStatus::Ready;
            
            std::cout << "✅ Successfully connected to camera " << cameraId 
                     << " (" << cameraInfo->serialNumber << ") with capture ready" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "Exception connecting to camera " << cameraId << ": " << e.what() << std::endl;
            return false;
        }
    }
    
    bool disconnectCamera(const std::string& cameraId) {
        std::cout << "Disconnecting camera: " << cameraId << std::endl;
        
        auto it = connectedCameras_.find(cameraId);
        if (it == connectedCameras_.end()) {
            std::cout << "Camera " << cameraId << " not connected" << std::endl;
            return true; // Not an error
        }
        
        try {
            // Cleanup Sapera objects in reverse order
            if (it->second.transfer) {
                it->second.transfer->Destroy();
                delete it->second.transfer;
            }
            if (it->second.buffer) {
                it->second.buffer->Destroy();
                delete it->second.buffer;
            }
            if (it->second.acqDevice) {
                it->second.acqDevice->Destroy();
                delete it->second.acqDevice;
            }
            
            // Remove from connected cameras
            connectedCameras_.erase(it);
            
            // Update discovered camera status
            for (auto& cam : discoveredCameras_) {
                if (cam.id == cameraId) {
                    cam.isConnected = false;
                    cam.status = CameraStatus::Disconnected;
                    break;
                }
            }
            
            std::cout << "✅ Successfully disconnected camera " << cameraId << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "Exception disconnecting camera " << cameraId << ": " << e.what() << std::endl;
            return false;
        }
    }
    
    bool captureImage(const std::string& cameraId, ImageBuffer& buffer) {
        std::cout << "Capturing image from camera: " << cameraId << std::endl;
        
        auto it = connectedCameras_.find(cameraId);
        if (it == connectedCameras_.end()) {
            std::cout << "Camera " << cameraId << " not connected" << std::endl;
            return false;
        }
        
        if (!it->second.captureReady) {
            std::cout << "Camera " << cameraId << " not ready for capture" << std::endl;
            return false;
        }
        
        try {
            auto& cam = it->second;
            
            std::cout << "Starting image capture..." << std::endl;
            
            // Perform capture (snap)
            if (!cam.transfer->Snap()) {
                std::cout << "Failed to start capture for camera " << cameraId << std::endl;
                return false;
            }
            
            std::cout << "Waiting for image..." << std::endl;
            
            // Wait for capture to complete (5 second timeout)
            if (!cam.transfer->Wait(5000)) {
                std::cout << "Capture timeout for camera " << cameraId << std::endl;
                cam.transfer->Abort();
                return false;
            }
            
            std::cout << "✅ Image captured successfully!" << std::endl;
            
            // Fill ImageBuffer with captured data
            buffer.width = cam.buffer->GetWidth();
            buffer.height = cam.buffer->GetHeight();
            buffer.pixelFormat = PixelFormat::BayerRG8; // Most common for these cameras
            buffer.timestamp = std::chrono::system_clock::now();
            buffer.metadata.cameraId = cameraId;
            buffer.metadata.serialNumber = cam.info.serialNumber;
            buffer.metadata.captureTime = buffer.timestamp;
            
            std::cout << "📊 Image info: " << buffer.width << "x" << buffer.height 
                     << " pixels, " << cam.buffer->GetPixelDepth() << " bits/pixel" << std::endl;
            
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "Exception during capture: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Save image to file (helper function)
    bool saveImage(const std::string& cameraId, const std::string& filename) {
        auto it = connectedCameras_.find(cameraId);
        if (it == connectedCameras_.end()) {
            std::cout << "Camera " << cameraId << " not connected" << std::endl;
            return false;
        }
        
        try {
            std::cout << "Saving image to: " << filename << std::endl;
            if (it->second.buffer->Save(filename.c_str(), "-format tiff")) {
                std::cout << "✅ Image saved successfully" << std::endl;
                return true;
            } else {
                std::cout << "❌ Failed to save image" << std::endl;
                return false;
            }
        } catch (const std::exception& e) {
            std::cout << "Exception saving image: " << e.what() << std::endl;
            return false;
        }
    }
    
    std::vector<std::string> getConnectedCameraIds() const {
        std::vector<std::string> ids;
        for (const auto& pair : connectedCameras_) {
            ids.push_back(pair.first);
        }
        return ids;
    }
    
    bool isConnected(const std::string& cameraId) const {
        return connectedCameras_.find(cameraId) != connectedCameras_.end();
    }
    
    // Cleanup all connections
    ~Impl() {
        std::cout << "Cleaning up SaperaInterface..." << std::endl;
        for (auto& pair : connectedCameras_) {
            if (pair.second.transfer) {
                pair.second.transfer->Destroy();
                delete pair.second.transfer;
            }
            if (pair.second.buffer) {
                pair.second.buffer->Destroy();
                delete pair.second.buffer;
            }
            if (pair.second.acqDevice) {
                pair.second.acqDevice->Destroy();
                delete pair.second.acqDevice;
            }
        }
        connectedCameras_.clear();
    }
};

// SaperaInterface implementation
SaperaInterface::SaperaInterface() : impl_(std::make_unique<Impl>()) {
    std::cout << "SaperaInterface created" << std::endl;
}

SaperaInterface::~SaperaInterface() {
    std::cout << "SaperaInterface destroyed" << std::endl;
}

bool SaperaInterface::initialize() {
    return impl_->initialize();
}

std::vector<CameraInfo> SaperaInterface::discoverCameras() {
    return impl_->discoverCameras();
}

bool SaperaInterface::connectCamera(const std::string& cameraId) {
    return impl_->connectCamera(cameraId);
}

bool SaperaInterface::disconnectCamera(const std::string& cameraId) {
    return impl_->disconnectCamera(cameraId);
}

bool SaperaInterface::captureImage(const std::string& cameraId, ImageBuffer& buffer) {
    return impl_->captureImage(cameraId, buffer);
}

bool SaperaInterface::setParameter(const std::string& cameraId, const CameraParameter& parameter) {
    std::cout << "Setting parameter " << parameter.name << " for camera " << cameraId << std::endl;
    // TODO: Implement parameter setting
    return true;
}

CameraParameter SaperaInterface::getParameter(const std::string& cameraId, const std::string& parameterName) {
    CameraParameter param;
    param.name = parameterName;
    std::cout << "Getting parameter " << parameterName << " for camera " << cameraId << std::endl;
    // TODO: Implement parameter getting
    return param;
}

CaptureStatistics SaperaInterface::getStatistics(const std::string& cameraId) {
    CaptureStatistics stats;
    stats.cameraId = cameraId;
    // TODO: Implement statistics
    return stats;
}

} // namespace SaperaCapturePro 