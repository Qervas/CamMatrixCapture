#ifndef CAMERA_CONFIG_MANAGER_HPP
#define CAMERA_CONFIG_MANAGER_HPP

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <functional>
#include "lib/json.hpp"
#include "sapclassbasic.h"

// Forward declarations
class SapAcqDevice;
class SapAcqDeviceToBuf;
class SapBuffer;

// Camera parameter structure
struct CameraParameters {
    int exposureTime = 40000;      // microseconds
    double gain = 1.0;             // sensor gain
    int blackLevel = 40;           // black level offset
    std::string pixelFormat = "BayerRG12";
    bool autoExposure = false;
    bool autoGain = false;
    
    // White balance
    double redBalance = 1.60156;
    double greenBalance = 1.0;
    double blueBalance = 1.89844;
    
    // Advanced parameters
    int acquisitionFrameRate = 4;
    std::string triggerMode = "Off";
    
    // Parameter validation ranges
    struct Limits {
        int minExposure = 500;     // From CCF: exposureAutoMinValue
        int maxExposure = 100000;  // Extended from CCF: exposureAutoMaxValue
        double minGain = 1.0;
        double maxGain = 4.0;      // From CCF: gainAutoMaxValue
    } limits;
};

// Camera position and identity info
struct CameraInfo {
    std::string serverName;
    std::string serialNumber;
    std::string modelName;
    int position = -1;
    bool isConnected = false;
    CameraParameters parameters;
};

// Configuration change callback type
using ParameterChangeCallback = std::function<void(const std::string& serialNumber, const CameraParameters& newParams)>;

// Add camera handle structure for integration with main system
struct WebCameraHandle {
    SapAcqDevice* acqDevice = nullptr;
    SapAcqDeviceToBuf* transfer = nullptr;
    SapBuffer* buffer = nullptr;
    bool isAvailable = false;
};

class CameraConfigManager {
public:
    // Singleton pattern for global access
    static CameraConfigManager& getInstance() {
        static CameraConfigManager instance;
        return instance;
    }

    // Configuration loading/saving
    bool loadFromFile(const std::string& filename);
    bool saveToFile(const std::string& filename) const;
    bool reloadConfiguration();

    // Camera management
    std::vector<CameraInfo> getConnectedCameras() const;
    bool addCamera(const CameraInfo& camera);
    bool removeCamera(const std::string& serialNumber);
    CameraInfo* getCamera(const std::string& serialNumber);
    CameraInfo* getCameraByPosition(int position);

    // Parameter management
    bool setParameter(const std::string& serialNumber, const std::string& paramName, const simple_json::JsonValue& value);
    simple_json::JsonValue getParameter(const std::string& serialNumber, const std::string& paramName) const;
    CameraParameters getParameters(const std::string& serialNumber) const;
    bool setParameters(const std::string& serialNumber, const CameraParameters& params);

    // Global default parameters
    void setDefaultParameters(const CameraParameters& params);
    CameraParameters getDefaultParameters() const;

    // Parameter validation
    bool validateParameter(const std::string& paramName, const simple_json::JsonValue& value) const;
    std::string getParameterValidationError(const std::string& paramName, const simple_json::JsonValue& value) const;

    // Parameter presets/templates
    bool savePreset(const std::string& presetName, const CameraParameters& params);
    bool loadPreset(const std::string& presetName, CameraParameters& params) const;
    std::vector<std::string> getAvailablePresets() const;

    // Change notifications
    void registerParameterChangeCallback(ParameterChangeCallback callback);
    void unregisterParameterChangeCallback();

    // Runtime parameter application
    bool applyParametersToCamera(const std::string& serialNumber, void* sapAcqDevice) const;
    
    // Camera capture operations
    bool captureFromCamera(const std::string& serialNumber, const std::string& outputDir = "captured_images", const std::string& format = "tiff");
    bool captureFromAllCameras(const std::string& outputDir = "captured_images", const std::string& format = "tiff");
    
    // Integration with existing camera systems (for real capture when cameras are available)
    bool captureFromExistingCamera(const std::string& serialNumber, void* existingSapAcqDevice, void* existingSapBuffer, const std::string& outputDir = "captured_images", const std::string& format = "tiff");
    
    // JSON API for server endpoints
    simple_json::JsonValue toJson() const;
    bool fromJson(const simple_json::JsonValue& json);
    simple_json::JsonValue getCameraListJson() const;
    simple_json::JsonValue getCameraParametersJson(const std::string& serialNumber) const;

    // Parameter change history (for debugging/logging)
    struct ParameterChange {
        std::string timestamp;
        std::string serialNumber;
        std::string parameterName;
        std::string oldValue;
        std::string newValue;
        std::string source; // "api", "config", "auto", etc.
    };
    
    std::vector<ParameterChange> getParameterHistory() const;
    void clearParameterHistory();

    // Resource management
    void clearAllCameras();
    bool validateConfiguration() const;
    
    // Quiet mode for batch operations
    void setQuietMode(bool quiet) { quietMode_ = quiet; }
    bool isQuietMode() const { return quietMode_; }
    
    // Add methods for camera handle integration
    void registerCameraHandle(const std::string& serialNumber, 
                             SapAcqDevice* acqDevice, 
                             SapAcqDeviceToBuf* transfer, 
                             SapBuffer* buffer);
    void unregisterCameraHandle(const std::string& serialNumber);
    bool hasRealCameraHandle(const std::string& serialNumber) const;

private:
    CameraConfigManager() = default;
    ~CameraConfigManager() = default;
    
    // Prevent copying
    CameraConfigManager(const CameraConfigManager&) = delete;
    CameraConfigManager& operator=(const CameraConfigManager&) = delete;

    // Thread safety
    mutable std::mutex configMutex_;
    
    // Data storage
    std::map<std::string, CameraInfo> cameras_;           // Key: serial number
    std::map<int, std::string> positionMap_;              // Key: position -> serial number
    CameraParameters defaultParameters_;
    std::map<std::string, CameraParameters> presets_;
    std::vector<ParameterChange> parameterHistory_;
    std::string configFilename_;
    
    // Callbacks
    ParameterChangeCallback parameterChangeCallback_;
    
    // Add storage for camera handles from main system
    std::map<std::string, WebCameraHandle> cameraHandles_;
    
    // Quiet mode flag
    bool quietMode_ = false;
    
    // Helper methods
    void notifyParameterChange(const std::string& serialNumber, const CameraParameters& newParams);
    void logParameterChange(const std::string& serialNumber, const std::string& paramName, 
                          const std::string& oldValue, const std::string& newValue, const std::string& source);
    bool isValidParameterName(const std::string& paramName) const;
    simple_json::JsonValue parameterToJson(const CameraParameters& params) const;
    CameraParameters parameterFromJson(const simple_json::JsonValue& json) const;
};

// Utility functions for parameter conversion
namespace ParameterUtils {
    std::string toString(const CameraParameters& params);
    bool fromString(const std::string& str, CameraParameters& params);
    
    // Parameter bounds checking
    bool isExposureValid(int exposureTime);
    bool isGainValid(double gain);
    bool isBlackLevelValid(int blackLevel);
    
    // Parameter application helpers
    bool applyExposureTime(void* sapAcqDevice, int exposureTime);
    bool applyGain(void* sapAcqDevice, double gain);
    bool applyBlackLevel(void* sapAcqDevice, int blackLevel);
    bool applyWhiteBalance(void* sapAcqDevice, double red, double green, double blue);
}

#endif // CAMERA_CONFIG_MANAGER_HPP 