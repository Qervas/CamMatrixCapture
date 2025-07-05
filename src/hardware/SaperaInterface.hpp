/**
 * SaperaInterface.hpp - Working Direct Sapera SDK Integration
 * Simple and proven camera interface
 */

#pragma once

#include "CameraTypes.hpp"
#include <memory>
#include <vector>
#include <string>

namespace SaperaCapturePro {

/**
 * Simple, working SaperaInterface for camera operations
 * Direct Sapera SDK integration without complex abstractions
 */
class SaperaInterface {
private:
    class Impl;
    std::unique_ptr<Impl> impl_;

public:
    SaperaInterface();
    ~SaperaInterface();

    // Core functionality - proven to work with real hardware
    bool initialize();
    std::vector<CameraInfo> discoverCameras();
    bool connectCamera(const std::string& cameraId);
    bool disconnectCamera(const std::string& cameraId);
    bool captureImage(const std::string& cameraId, ImageBuffer& buffer);
    
    // Parameter control (TODO - implement incrementally)
    bool setParameter(const std::string& cameraId, const CameraParameter& parameter);
    CameraParameter getParameter(const std::string& cameraId, const std::string& parameterName);
    
    // Statistics (TODO - implement incrementally)
    CaptureStatistics getStatistics(const std::string& cameraId);
    
    // Additional helper methods that can be added
    // std::vector<std::string> getConnectedCameraIds() const;
    // bool isConnected(const std::string& cameraId) const;
    // bool saveImage(const std::string& cameraId, const std::string& filename);
};

} // namespace SaperaCapturePro 