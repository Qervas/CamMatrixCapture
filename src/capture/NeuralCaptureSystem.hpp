#pragma once

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
#include <fstream>
#include <ctime>

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

// Forward declare ParameterController
class ParameterController;

class NeuralRenderingCaptureSystem {
private:
    std::vector<CameraInfo> discoveredCameras_;
    std::map<std::string, ConnectedCamera> connectedCameras_;
    std::string datasetPath_;
    CaptureFormat currentFormat_;
    int captureCounter_;
    std::string currentSessionName_;
    int exposureTime_; // Exposure time in microseconds
    std::unique_ptr<ParameterController> parameterController_; // Enhanced parameter control

public:
    NeuralRenderingCaptureSystem(const std::string& datasetPath = "neural_dataset");
    ~NeuralRenderingCaptureSystem();
    
    // Core functionality
    std::vector<CameraInfo> discoverCameras();
    bool connectAllCameras();
    bool connectCamera(const std::string& cameraId);
    bool captureAllCameras();
    bool captureSingleCamera(const std::string& cameraId, const std::string& sessionPath);
    
    // Parameter control
    bool setExposureTime(int exposureTimeUs);
    int getExposureTime() const;
    
    // Configuration
    void setFormat(CaptureFormat format);
    void setDatasetPath(const std::string& path);
    void resetCaptureCounter();
    
    // Status
    void printCameraStatus();
    
    // Getters for GUI
    const std::vector<CameraInfo>& getDiscoveredCameras() const { return discoveredCameras_; }
    const std::map<std::string, ConnectedCamera>& getConnectedCameras() const { return connectedCameras_; }
    std::string getDatasetPath() const { return datasetPath_; }
    CaptureFormat getCurrentFormat() const { return currentFormat_; }
    int getCaptureCounter() const { return captureCounter_; }
    
private:
    bool applyExposureTime(SapAcqDevice* acqDevice, int exposureTimeUs);
    std::string generateSessionName(int captureNumber);
    std::string generateImageFilename(const std::string& cameraName, int captureNumber);
    void saveSessionMetadata(const std::string& sessionName, int captureNumber, bool success);
}; 