/**
 * CaptureEngine.cpp - Simple Working Image Capture Engine
 * Wrapper around our proven SaperaInterface
 */

#include "CaptureEngine.hpp"
#include <iostream>

namespace SaperaCapturePro {

class CaptureEngine::Impl {
private:
    SaperaInterface saperaInterface_;
    std::vector<CameraInfo> discoveredCameras_;
    bool initialized_ = false;

public:
    bool initialize() {
        std::cout << "Initializing CaptureEngine..." << std::endl;
        
        if (!saperaInterface_.initialize()) {
            std::cout << "Failed to initialize SaperaInterface" << std::endl;
            return false;
        }
        
        // Discover cameras
        discoveredCameras_ = saperaInterface_.discoverCameras();
        
        if (discoveredCameras_.empty()) {
            std::cout << "No cameras discovered" << std::endl;
            return false;
        }
        
        initialized_ = true;
        std::cout << "✅ CaptureEngine initialized with " << discoveredCameras_.size() << " cameras" << std::endl;
        return true;
    }
    
    std::vector<CameraInfo> listCameras() {
        if (!initialized_) {
            std::cout << "CaptureEngine not initialized" << std::endl;
            return {};
        }
        
        return discoveredCameras_;
    }
    
    bool connectCamera(const std::string& cameraId) {
        if (!initialized_) {
            std::cout << "CaptureEngine not initialized" << std::endl;
            return false;
        }
        
        return saperaInterface_.connectCamera(cameraId);
    }
    
    bool disconnectCamera(const std::string& cameraId) {
        if (!initialized_) {
            std::cout << "CaptureEngine not initialized" << std::endl;
            return false;
        }
        
        return saperaInterface_.disconnectCamera(cameraId);
    }
    
    bool startCapture(const std::string& cameraId, const CaptureSettings& settings) {
        std::cout << "Starting capture for camera: " << cameraId << std::endl;
        
        if (!initialized_) {
            std::cout << "CaptureEngine not initialized" << std::endl;
            return false;
        }
        
        // For now, just ensure camera is connected
        // TODO: Apply capture settings
        return saperaInterface_.connectCamera(cameraId);
    }
    
    bool stopCapture(const std::string& cameraId) {
        std::cout << "Stopping capture for camera: " << cameraId << std::endl;
        
        if (!initialized_) {
            std::cout << "CaptureEngine not initialized" << std::endl;
            return false;
        }
        
        // For now, just disconnect camera
        return saperaInterface_.disconnectCamera(cameraId);
    }
    
    bool captureImage(const std::string& cameraId, ImageBuffer& buffer) {
        if (!initialized_) {
            std::cout << "CaptureEngine not initialized" << std::endl;
            return false;
        }
        
        return saperaInterface_.captureImage(cameraId, buffer);
    }
    
    bool setParameter(const std::string& cameraId, const CameraParameter& parameter) {
        if (!initialized_) {
            std::cout << "CaptureEngine not initialized" << std::endl;
            return false;
        }
        
        return saperaInterface_.setParameter(cameraId, parameter);
    }
    
    CameraParameter getParameter(const std::string& cameraId, const std::string& parameterName) {
        if (!initialized_) {
            std::cout << "CaptureEngine not initialized" << std::endl;
            return CameraParameter{};
        }
        
        return saperaInterface_.getParameter(cameraId, parameterName);
    }
    
    CaptureStatistics getStatistics(const std::string& cameraId) {
        if (!initialized_) {
            std::cout << "CaptureEngine not initialized" << std::endl;
            return CaptureStatistics{};
        }
        
        return saperaInterface_.getStatistics(cameraId);
    }
    
    CameraHealth getHealth(const std::string& cameraId) {
        CameraHealth health;
        health.cameraId = cameraId;
        health.isHealthy = true; // Simple implementation for now
        health.temperature = 42.0; // Default value
        health.lastCheck = std::chrono::system_clock::now();
        
        // TODO: Get real health information from camera
        return health;
    }
    
    bool isInitialized() const {
        return initialized_;
    }
};

// CaptureEngine implementation
CaptureEngine::CaptureEngine() : impl_(std::make_unique<Impl>()) {
    std::cout << "CaptureEngine created" << std::endl;
}

CaptureEngine::~CaptureEngine() {
    std::cout << "CaptureEngine destroyed" << std::endl;
}

bool CaptureEngine::initialize() {
    return impl_->initialize();
}

std::vector<CameraInfo> CaptureEngine::listCameras() {
    return impl_->listCameras();
}

bool CaptureEngine::connectCamera(const std::string& cameraId) {
    return impl_->connectCamera(cameraId);
}

bool CaptureEngine::disconnectCamera(const std::string& cameraId) {
    return impl_->disconnectCamera(cameraId);
}

bool CaptureEngine::startCapture(const std::string& cameraId, const CaptureSettings& settings) {
    return impl_->startCapture(cameraId, settings);
}

bool CaptureEngine::stopCapture(const std::string& cameraId) {
    return impl_->stopCapture(cameraId);
}

bool CaptureEngine::captureImage(const std::string& cameraId, ImageBuffer& buffer) {
    return impl_->captureImage(cameraId, buffer);
}

bool CaptureEngine::setParameter(const std::string& cameraId, const CameraParameter& parameter) {
    return impl_->setParameter(cameraId, parameter);
}

CameraParameter CaptureEngine::getParameter(const std::string& cameraId, const std::string& parameterName) {
    return impl_->getParameter(cameraId, parameterName);
}

CaptureStatistics CaptureEngine::getStatistics(const std::string& cameraId) {
    return impl_->getStatistics(cameraId);
}

CameraHealth CaptureEngine::getHealth(const std::string& cameraId) {
    return impl_->getHealth(cameraId);
}

bool CaptureEngine::isInitialized() const {
    return impl_->isInitialized();
}

} // namespace SaperaCapturePro 