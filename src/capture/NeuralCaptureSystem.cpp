#include "capture/NeuralCaptureSystem.hpp"
#include "capture/ParameterController.hpp"
#include <SapManager.h>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>

// Constructor
NeuralRenderingCaptureSystem::NeuralRenderingCaptureSystem(const std::string& datasetPath)
    : datasetPath_(datasetPath),
      currentFormat_(CaptureFormat::TIFF),
      captureCounter_(0),
      exposureTime_(10000) { // Default exposure time in microseconds
    parameterController_ = std::make_unique<ParameterController>();
}

// Destructor
NeuralRenderingCaptureSystem::~NeuralRenderingCaptureSystem() {
    std::cout << "🧹 Shutting down: Releasing all Sapera resources..." << std::endl;
    destroyAllCameraObjects();
    std::cout << "✅ System cleanup complete." << std::endl;
}

// Final cleanup function to release all Sapera objects.
// This is the "hard" disconnect, only called on destruction.
void NeuralRenderingCaptureSystem::destroyAllCameraObjects() {
    std::cout << "Destroying all persistent camera objects..." << std::endl;

    for (auto& camera : cameras_) {
        // Stop and destroy the transfer object first.
        if (camera.transfer) {
            if (camera.transfer->IsCreated()) {
                camera.transfer->Freeze(); // Safe to call even if not grabbing
                camera.transfer->Destroy();
            }
            delete camera.transfer;
            camera.transfer = nullptr;
        }

        // Destroy buffer if it exists
        if (camera.buffer) {
            if (camera.buffer->IsCreated()) {
                camera.buffer->Destroy();
            }
            delete camera.buffer;
            camera.buffer = nullptr;
        }
        
        // Destroy acquisition device if it exists
        if (camera.acqDevice) {
            if (camera.acqDevice->IsCreated()) {
                camera.acqDevice->Destroy();
            }
            delete camera.acqDevice;
            camera.acqDevice = nullptr;
        }

        camera.isInitialized = false;
        camera.isConnected = false;
    }
    std::cout << "All camera objects have been destroyed." << std::endl;
}


// --- Core Workflow ---

// Step 1: Find all available hardware. Populates the camera list with basic info.
bool NeuralRenderingCaptureSystem::discoverCameras() {
    cameras_.clear(); // Clear previous discovery results
    std::cout << "🔍 Discovering Sapera devices..." << std::endl;
    int serverCount = SapManager::GetServerCount();
    if (serverCount == 0) {
        std::cout << "No Sapera servers found." << std::endl;
        return false;
    }

    for (int serverIndex = 0; serverIndex < serverCount; ++serverIndex) {
        char serverName[CORSERVER_MAX_STRLEN];
        SapManager::GetServerName(serverIndex, serverName, CORSERVER_MAX_STRLEN);
        int deviceCount = SapManager::GetResourceCount(serverName, SapManager::ResourceAcqDevice);

        for (int deviceIndex = 0; deviceIndex < deviceCount; ++deviceIndex) {
            CameraInfo cam;
            cam.serverName = serverName;
            cam.serverIndex = serverIndex;
            cam.deviceIndex = deviceIndex;
            cameras_.push_back(cam);
        }
    }
    std::cout << "✅ Discovered " << cameras_.size() << " potential camera devices." << std::endl;
    return true;
}

// Step 2: Creates the underlying Sapera objects (AcqDevice, Buffer) for all discovered cameras.
bool NeuralRenderingCaptureSystem::initializeAllCameras() {
    std::cout << "🛠️ Initializing all discovered cameras (creating persistent objects)..." << std::endl;
    bool allSuccess = true;
    for (auto& camera : cameras_) {
        if (camera.isInitialized) continue; // Skip if already done

        // Create Acquisition Device
        camera.acqDevice = new SapAcqDevice(SapLocation(camera.serverName.c_str(), camera.deviceIndex), FALSE);
        if (!camera.acqDevice || !camera.acqDevice->Create()) {
            std::cerr << "❌ Failed to create SapAcqDevice for " << camera.serverName << " [" << camera.deviceIndex << "]" << std::endl;
            delete camera.acqDevice;
            camera.acqDevice = nullptr;
            allSuccess = false;
            continue;
        }

        // Get device info
        camera.acqDevice->GetFeatureValue("DeviceModelName", camera.modelName, MAX_PATH);
        camera.acqDevice->GetFeatureValue("DeviceID", camera.serialNumber, MAX_PATH);
        camera.name = camera.modelName + " (" + camera.serialNumber + ")";

        // Create Buffer
        camera.buffer = new SapBuffer(1, camera.acqDevice);
        if (!camera.buffer || !camera.buffer->Create()) {
            std::cerr << "❌ Failed to create SapBuffer for " << camera.name << std::endl;
            camera.acqDevice->Destroy();
            delete camera.acqDevice;
            camera.acqDevice = nullptr;
            allSuccess = false;
            continue;
        }

        camera.isInitialized = true;
        std::cout << "  -> Initialized: " << camera.name << std::endl;

        // Apply default or current settings now that the device object exists
        applyExposureTime(camera.acqDevice, exposureTime_);
    }
    return allSuccess;
}

// Step 3: Connects all initialized cameras by creating a transfer and starting the grab.
bool NeuralRenderingCaptureSystem::connectAllCameras() {
    std::cout << "🔌 Connecting all initialized cameras..." << std::endl;
    bool allSuccess = true;
    for (auto& camera : cameras_) {
        if (!camera.isInitialized || camera.isConnected) {
            continue; // Skip uninitialized or already connected cameras
        }

        camera.transfer = new SapAcqDeviceToBuf(camera.acqDevice, camera.buffer);
        if (!camera.transfer || !camera.transfer->Create()) {
            std::cerr << "❌ Failed to create SapTransfer for " << camera.name << std::endl;
            delete camera.transfer;
            camera.transfer = nullptr;
            allSuccess = false;
            continue;
        }
        
        // Start grabbing frames
        if (!camera.transfer->Grab()) {
             std::cerr << "❌ Failed to start grabbing on " << camera.name << std::endl;
             allSuccess = false;
             camera.transfer->Destroy();
             delete camera.transfer;
             camera.transfer = nullptr;
             continue;
        }

        camera.isConnected = true;
        std::cout << "  -> Connected and grabbing: " << camera.name << std::endl;
    }
    return allSuccess;
}

// Step 4: Triggers a capture on all currently connected cameras.
bool NeuralRenderingCaptureSystem::captureAllCameras() {
    if (getConnectedCameraCount() == 0) {
        std::cerr << "❌ No cameras are connected to capture from." << std::endl;
        return false;
    }
    
    std::string sessionName = generateSessionName(captureCounter_);
    std::string sessionPath = datasetPath_ + "/" + sessionName;

    try {
        std::filesystem::create_directories(sessionPath + "/images");
    } catch (const std::exception& e) {
        std::cerr << "❌ Error creating directory: " << sessionPath << " - " << e.what() << std::endl;
        return false;
    }

    std::cout << "🎬 Starting capture for session: " << sessionName << std::endl;
    bool allSuccess = true;
    for (auto& camera : cameras_) {
        if (camera.isConnected) {
            if (!captureSingleCamera(camera, sessionPath)) {
                allSuccess = false;
            }
        }
    }

    saveSessionMetadata(sessionName, captureCounter_, allSuccess);
    if(allSuccess) {
        captureCounter_++;
        std::cout << "✅ Capture successful." << std::endl;
    } else {
        std::cerr << "⚠️ Capture failed for one or more cameras." << std::endl;
    }
    
    return allSuccess;
}


// --- Parameter & Configuration ---
bool NeuralRenderingCaptureSystem::setExposureTime(int exposureTimeUs) {
    std::cout << "Setting exposure time to " << exposureTimeUs << " us for all initialized cameras." << std::endl;
    exposureTime_ = exposureTimeUs;
    bool allSuccess = true;

    for (auto& camera : cameras_) {
        // Apply to any camera that has its acquisition device created.
        if (camera.isInitialized && camera.acqDevice) {
            if (!applyExposureTime(camera.acqDevice, exposureTimeUs)) {
                allSuccess = false;
                std::cerr << "Failed to set exposure for " << camera.name << std::endl;
            }
        }
    }
    return allSuccess;
}

int NeuralRenderingCaptureSystem::getExposureTime() const {
    return exposureTime_;
}

void NeuralRenderingCaptureSystem::setFormat(CaptureFormat format) {
    currentFormat_ = format;
}

void NeuralRenderingCaptureSystem::setDatasetPath(const std::string& path) {
    datasetPath_ = path;
}

void NeuralRenderingCaptureSystem::resetCaptureCounter() {
    captureCounter_ = 0;
    std::cout << "Capture counter reset to 0." << std::endl;
}


// --- Private Helper Functions ---
bool NeuralRenderingCaptureSystem::applyExposureTime(SapAcqDevice* acqDevice, int exposureTimeUs) {
    if (!acqDevice) return false;
    if (acqDevice->IsFeatureAvailable("ExposureTime")) {
        return acqDevice->SetFeatureValue("ExposureTime", (double)exposureTimeUs);
    }
    std::cerr << "Warning: 'ExposureTime' feature not available on device " << acqDevice->GetResourceName() << std::endl;
    return false;
}

bool NeuralRenderingCaptureSystem::captureSingleCamera(CameraInfo& camera, const std::string& sessionPath) {
    if (!camera.transfer) {
        std::cerr << "❌ Cannot capture, camera " << camera.name << " has no transfer object." << std::endl;
        return false;
    }
    
    // The camera is already grabbing continuously. We just need to wait for a new frame to arrive.
    if (!camera.transfer->Wait(2000)) {
        std::cerr << "❌ Timed out waiting for a new frame from " << camera.name << std::endl;
        return false;
    }

    std::string filename = generateImageFilename(camera, camera.deviceIndex);
    std::string fullPath = sessionPath + "/images/" + filename;
    
    std::string options = (currentFormat_ == CaptureFormat::TIFF) ? "-format tiff" : "-format raw";

    if (!camera.buffer->Save(fullPath.c_str(), options.c_str())) {
        std::cerr << "❌ Failed to save image for " << camera.name << " to " << fullPath << std::endl;
        return false;
    }
    
    return true;
}

std::string NeuralRenderingCaptureSystem::generateSessionName(int captureNumber) {
    auto now = std::chrono::system_clock::now();
    time_t in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &in_time_t);
#else
    localtime_r(&in_time_t, &tm_buf);
#endif
    std::stringstream ss;
    ss << "capture_" << std::setfill('0') << std::setw(4) << captureNumber << "_"
       << std::put_time(&tm_buf, "%Y%m%d_%H%M%S");
    return ss.str();
}

std::string NeuralRenderingCaptureSystem::generateImageFilename(const CameraInfo& camera, int cameraIndex) {
    std::string format_ext = (currentFormat_ == CaptureFormat::TIFF) ? ".tiff" : ".raw";
    return "cam" + std::to_string(cameraIndex) + format_ext;
}

void NeuralRenderingCaptureSystem::saveSessionMetadata(const std::string& sessionName, int captureNumber, bool success) {
    std::string metaPath = datasetPath_ + "/" + sessionName + "/metadata.txt";
    std::ofstream metadataFile(metaPath);
    if (!metadataFile.is_open()) {
        std::cerr << "Failed to create metadata file at " << metaPath << std::endl;
        return;
    }
    auto now = std::chrono::system_clock::now();
    time_t in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &in_time_t);
#else
    localtime_r(&in_time_t, &tm_buf);
#endif

    metadataFile << "SessionName: " << sessionName << std::endl;
    metadataFile << "Timestamp: " << std::put_time(&tm_buf, "%Y-%m-%d %X") << std::endl;
    metadataFile << "Success: " << (success ? "true" : "false") << std::endl;
    metadataFile << "Format: " << (currentFormat_ == CaptureFormat::TIFF ? "TIFF" : "RAW") << std::endl;
    metadataFile << "ExposureTime_us: " << exposureTime_ << std::endl;
    metadataFile << "\n[ConnectedCameras]\n";
    int camIndex = 0;
    for (const auto& cam : cameras_) {
        if(cam.isConnected) {
            metadataFile << "camera_" << camIndex << "_name=" << cam.name << std::endl;
            metadataFile << "camera_" << camIndex << "_serial=" << cam.serialNumber << std::endl;
        }
        camIndex++;
    }
    metadataFile.close();
}

// --- Status & Getters ---
int NeuralRenderingCaptureSystem::getConnectedCameraCount() const {
    int count = 0;
    for(const auto& cam : cameras_) {
        if(cam.isConnected) {
            count++;
        }
    }
    return count;
}