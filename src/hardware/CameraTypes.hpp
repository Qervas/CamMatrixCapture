/**
 * CameraTypes.hpp - Simplified Camera Type Definitions
 * Based on working simple_camera_test.cpp approach
 */

#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <variant>

namespace SaperaCapturePro {

// Simple camera status enum
enum class CameraStatus {
    Disconnected,
    Connected,
    Initializing,
    Ready,
    Capturing,
    Error
};

// Simple camera type enum
enum class CameraType {
    Area,
    Line,
    Industrial
};

// Simple pixel format enum
enum class PixelFormat {
    Mono8,
    Mono10,
    Mono12,
    RGB8,
    BayerRG8
};

// Simple trigger mode enum
enum class TriggerMode {
    Off,
    Software,
    Hardware
};

// String conversion functions
std::string toString(CameraStatus status);
std::string toString(CameraType type);
std::string toString(PixelFormat format);
std::string toString(TriggerMode mode);

// Camera parameter
struct CameraParameter {
    enum class Type {
        Integer,
        Float,
        Boolean,
        String,
        Enumeration
    };
    
    std::string name;
    Type type = Type::String;
    std::variant<int, double, bool, std::string> value;
    bool readOnly = false;
    
    bool setValue(const std::string& value);
    std::string getValue() const;
};

// Camera capabilities
struct CameraCapabilities {
    std::vector<PixelFormat> supportedPixelFormats;
    int minExposureTime = 1000;
    int maxExposureTime = 1000000;
    double minGain = 1.0;
    double maxGain = 4.0;
    int maxWidth = 4096;
    int maxHeight = 3008;
    bool supportsTrigger = true;
    bool supportsHardwareTrigger = true;
    bool supportsSoftwareTrigger = true;
};

// Image metadata
struct ImageMetadata {
    std::string cameraId;
    std::string serialNumber;
    int exposureTime = 0;
    double gain = 1.0;
    double temperature = 0.0;
    std::chrono::system_clock::time_point captureTime;
};

// Image buffer
struct ImageBuffer {
    uint32_t width = 0;
    uint32_t height = 0;
    PixelFormat pixelFormat = PixelFormat::Mono8;
    std::vector<uint8_t> data;
    std::chrono::system_clock::time_point timestamp;
    ImageMetadata metadata;
    
    ImageBuffer();
    ImageBuffer(uint32_t w, uint32_t h, PixelFormat format);
    size_t getDataSize() const;
};

// Camera information
struct CameraInfo {
    std::string id;
    std::string name;
    std::string serialNumber;
    std::string modelName;
    std::string serverName;
    int resourceIndex = 0;
    bool isConnected = false;
    CameraStatus status = CameraStatus::Disconnected;
    CameraType type = CameraType::Industrial;
    CameraCapabilities capabilities;
};

// Capture statistics
struct CaptureStatistics {
    std::string cameraId;
    int totalCaptured = 0;
    int successfulCaptures = 0;
    int failedCaptures = 0;
    double averageFPS = 0.0;
    std::chrono::system_clock::time_point lastCaptureTime;
    
    // Internal tracking
    std::chrono::high_resolution_clock::time_point lastFPSUpdate = std::chrono::high_resolution_clock::now();
    int recentCaptures = 0;
    
    void updateCapture(bool success, double captureTimeMs = 0.0);
    void reset();
    double getSuccessRate() const;
};

} // namespace SaperaCapturePro 