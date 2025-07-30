#pragma once

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

// Forward declare controllers
class ParameterController;
class AutoExposureController;

// Enum for image format
enum class CaptureFormat {
    TIFF,
    RAW
};

// Main struct to hold all information and resources for a single camera.
// The Sapera objects are owned by this struct and their lifetime is managed by the capture system.
struct CameraInfo {
    // Discovery Info
    std::string serverName;
    int serverIndex = -1;
    int deviceIndex = -1;

    // Detailed Info
    std::string name;
    std::string modelName;
    std::string serialNumber;

    // State Flags
    bool isInitialized = false; // SapAcqDevice and SapBuffer are created
    bool isConnected = false;   // SapTransfer is created and grabbing (or ready to)

    // Sapera Handles
    // These are kept alive even when "disconnected" to allow for parameter changes.
    SapAcqDevice* acqDevice = nullptr;
    SapBuffer* buffer = nullptr;
    SapAcqDeviceToBuf* transfer = nullptr;
};


class NeuralRenderingCaptureSystem {
public:
    NeuralRenderingCaptureSystem(const std::string& datasetPath = "neural_dataset");
    ~NeuralRenderingCaptureSystem();

    // --- Core Workflow ---

    // Step 1: Find all available hardware. Populates the camera list with basic info.
    bool discoverCameras();
    
    // Step 2: Creates the underlying Sapera objects (AcqDevice, Buffer) for all discovered cameras.
    // This prepares them for connection or parameter changes.
    bool initializeAllCameras();

    // Step 3: "Soft" connect. Creates the transfer object and starts grabbing.
    // Assumes cameras are initialized.
    bool connectAllCameras();



    // Step 5: Triggers a capture on all currently connected cameras.
    bool captureAllCameras();
    
    // --- Parameter & Configuration ---
    
    bool setExposureTime(int exposureTimeUs);
    int getExposureTime() const;
    void setFormat(CaptureFormat format);
    void setDatasetPath(const std::string& path);
    void resetCaptureCounter();

    // --- Status & Getters ---
    
    // Returns the master list of all cameras and their current states.
    const std::vector<CameraInfo>& getDiscoveredCameras() const { return cameras_; }
    std::string getDatasetPath() const { return datasetPath_; }
    CaptureFormat getCurrentFormat() const { return currentFormat_; }
    int getCaptureCounter() const { return captureCounter_; }
    std::string generateSessionName(int captureNumber); // Make public for GUI use


private:
    // --- Private Member Variables ---
    
    std::vector<CameraInfo> cameras_; // The single, authoritative list of all cameras
    std::string datasetPath_;
    CaptureFormat currentFormat_;
    int captureCounter_ = 0;
    int exposureTime_;

    std::unique_ptr<ParameterController> parameterController_;
    // std::unique_ptr<AutoExposureController> autoExposureController_; // Optional
    
    // --- Private Helper Functions ---
    
    // The final cleanup function, called by the destructor to release all resources.
    void destroyAllCameraObjects(); 
    
    bool captureSingleCamera(CameraInfo& camera, const std::string& sessionPath);
    bool applyExposureTime(SapAcqDevice* acqDevice, int exposureTimeUs);
    std::string generateImageFilename(const CameraInfo& camera, int cameraIndex);
    void saveSessionMetadata(const std::string& sessionName, int captureNumber, bool success);
};