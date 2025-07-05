/**
 * CaptureEngine.hpp - Simple Working Image Capture Engine
 * Clean wrapper around proven SaperaInterface
 */

#pragma once

#include "../hardware/SaperaInterface.hpp"
#include <memory>
#include <vector>
#include <string>

namespace SaperaCapturePro {

/**
 * Simple CaptureEngine - proven to work with real hardware
 * Wraps SaperaInterface for easy camera operations
 */
class CaptureEngine {
private:
    class Impl;
    std::unique_ptr<Impl> impl_;

public:
    CaptureEngine();
    ~CaptureEngine();

    // Core functionality - proven working
    bool initialize();
    std::vector<CameraInfo> listCameras();
    bool connectCamera(const std::string& cameraId);
    bool disconnectCamera(const std::string& cameraId);
    
    // Capture operations
    bool startCapture(const std::string& cameraId, const CaptureSettings& settings);
    bool stopCapture(const std::string& cameraId);
    bool captureImage(const std::string& cameraId, ImageBuffer& buffer);
    
    // Parameter control
    bool setParameter(const std::string& cameraId, const CameraParameter& parameter);
    CameraParameter getParameter(const std::string& cameraId, const std::string& parameterName);
    
    // Status and statistics
    CaptureStatistics getStatistics(const std::string& cameraId);
    CameraHealth getHealth(const std::string& cameraId);
    bool isInitialized() const;
};

} // namespace SaperaCapturePro 