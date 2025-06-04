#include "CameraConfigManager.hpp"
#include "sapclassbasic.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <sys/stat.h>
#endif

// Implementation of CameraConfigManager

bool CameraConfigManager::loadFromFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    try {
        configFilename_ = filename;
        auto config = simple_json::JsonParser::parse_file(filename);
        
        // Clear existing data
        cameras_.clear();
        positionMap_.clear();
        
        // Load default settings FIRST before creating cameras
        if (config["camera_settings"]["default"].is_object()) {
            const auto& defaults = config["camera_settings"]["default"];
            defaultParameters_.exposureTime = defaults["exposure_time"].get_int();
            defaultParameters_.gain = defaults["gain"].get_number();
            
            // Load auto exposure settings
            if (defaults.is_object() && defaults.get_object().find("auto_exposure") != defaults.get_object().end() && defaults["auto_exposure"].is_boolean()) {
                defaultParameters_.autoExposure = defaults["auto_exposure"].get_boolean();
            }
            if (defaults.is_object() && defaults.get_object().find("auto_gain") != defaults.get_object().end() && defaults["auto_gain"].is_boolean()) {
                defaultParameters_.autoGain = defaults["auto_gain"].get_boolean();
            }
            
            if (!quietMode_) {
                std::cout << "📋 Loaded default parameters: " << defaultParameters_.exposureTime << "μs exposure, " << defaultParameters_.gain << " gain" << std::endl;
                std::cout << "📋 Auto modes: Exposure=" << (defaultParameters_.autoExposure ? "ON" : "OFF") << ", Gain=" << (defaultParameters_.autoGain ? "ON" : "OFF") << std::endl;
            }
        }
        
        // Now create cameras with the correct default parameters
        for (const auto& position : config["camera_positions"].get_array()) {
            const auto& posObj = position.get_object();
            
            CameraInfo camera;
            camera.position = posObj.at("position").get_int();
            camera.serialNumber = posObj.at("full_serial").get_string();
            camera.isConnected = false;
            
            // Load default parameters (now loaded from config)
            camera.parameters = defaultParameters_;
            
            // Add to storage
            cameras_[camera.serialNumber] = camera;
            positionMap_[camera.position] = camera.serialNumber;
        }
        
        // Load camera-specific overrides
        if (config["camera_settings"]["overrides"].is_object()) {
            const auto& overrides = config["camera_settings"]["overrides"].get_object();
            for (const auto& pair : overrides) {
                const std::string& serial = pair.first;
                auto it = cameras_.find(serial);
                if (it != cameras_.end()) {
                    try {
                        if (pair.second.is_object() && pair.second["exposure_time"].is_number()) {
                            it->second.parameters.exposureTime = pair.second["exposure_time"].get_int();
                            if (!quietMode_) {
                                std::cout << "🔧 Override for " << serial << ": " << it->second.parameters.exposureTime << "μs exposure" << std::endl;
                            }
                        }
                    } catch (...) {}
                    
                    try {
                        if (pair.second.is_object() && pair.second["gain"].is_number()) {
                            it->second.parameters.gain = pair.second["gain"].get_number();
                            if (!quietMode_) {
                                std::cout << "🔧 Override for " << serial << ": " << it->second.parameters.gain << " gain" << std::endl;
                            }
                        }
                    } catch (...) {}
                    
                    try {
                        if (pair.second.is_object() && pair.second.get_object().find("auto_exposure") != pair.second.get_object().end() && pair.second["auto_exposure"].is_boolean()) {
                            it->second.parameters.autoExposure = pair.second["auto_exposure"].get_boolean();
                            if (!quietMode_) {
                                std::cout << "🔧 Override for " << serial << ": Auto exposure " << (it->second.parameters.autoExposure ? "ON" : "OFF") << std::endl;
                            }
                        }
                    } catch (...) {}
                    
                    try {
                        if (pair.second.is_object() && pair.second.get_object().find("auto_gain") != pair.second.get_object().end() && pair.second["auto_gain"].is_boolean()) {
                            it->second.parameters.autoGain = pair.second["auto_gain"].get_boolean();
                            if (!quietMode_) {
                                std::cout << "🔧 Override for " << serial << ": Auto gain " << (it->second.parameters.autoGain ? "ON" : "OFF") << std::endl;
                            }
                        }
                    } catch (...) {}
                }
            }
        }
        
        if (!quietMode_) {
            std::cout << "✓ Loaded configuration for " << cameras_.size() << " cameras from " << filename << std::endl;
        }
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Failed to load camera configuration: " << e.what() << std::endl;
        return false;
    }
}

bool CameraConfigManager::saveToFile(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    // For now, we'll just print what would be saved
    // In a full implementation, we'd generate JSON and write to file
    std::cout << "Would save configuration to: " << filename << std::endl;
    return true;
}

std::vector<CameraInfo> CameraConfigManager::getConnectedCameras() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    std::vector<CameraInfo> result;
    for (const auto& pair : cameras_) {
        if (pair.second.isConnected) {
            result.push_back(pair.second);
        }
    }
    
    // Sort by position
    std::sort(result.begin(), result.end(), [](const CameraInfo& a, const CameraInfo& b) {
        return a.position < b.position;
    });
    
    return result;
}

bool CameraConfigManager::addCamera(const CameraInfo& camera) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    cameras_[camera.serialNumber] = camera;
    if (camera.position > 0) {
        positionMap_[camera.position] = camera.serialNumber;
    }
    
    return true;
}

CameraInfo* CameraConfigManager::getCamera(const std::string& serialNumber) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    auto it = cameras_.find(serialNumber);
    return (it != cameras_.end()) ? &it->second : nullptr;
}

CameraInfo* CameraConfigManager::getCameraByPosition(int position) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    auto it = positionMap_.find(position);
    if (it != positionMap_.end()) {
        return getCamera(it->second);
    }
    return nullptr;
}

CameraParameters CameraConfigManager::getParameters(const std::string& serialNumber) const {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    auto it = cameras_.find(serialNumber);
    if (it != cameras_.end()) {
        return it->second.parameters;
    }
    return defaultParameters_;
}

bool CameraConfigManager::setParameters(const std::string& serialNumber, const CameraParameters& params) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    auto it = cameras_.find(serialNumber);
    if (it != cameras_.end()) {
        auto oldParams = it->second.parameters;
        it->second.parameters = params;
        
        // Log the change
        logParameterChange(serialNumber, "all_parameters", "bulk_change", "bulk_change", "api");
        
        // Notify callback if registered
        if (parameterChangeCallback_) {
            parameterChangeCallback_(serialNumber, params);
        }
        
        return true;
    }
    return false;
}

bool CameraConfigManager::setParameter(const std::string& serialNumber, const std::string& paramName, const simple_json::JsonValue& value) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    auto it = cameras_.find(serialNumber);
    if (it == cameras_.end()) {
        return false;
    }
    
    auto& params = it->second.parameters;
    std::string oldValue, newValue;
    
    try {
        if (paramName == "exposureTime" && value.is_number()) {
            oldValue = std::to_string(params.exposureTime);
            params.exposureTime = value.get_int();
            newValue = std::to_string(params.exposureTime);
        }
        else if (paramName == "gain" && value.is_number()) {
            oldValue = std::to_string(params.gain);
            params.gain = value.get_number();
            newValue = std::to_string(params.gain);
        }
        else if (paramName == "blackLevel" && value.is_number()) {
            oldValue = std::to_string(params.blackLevel);
            params.blackLevel = value.get_int();
            newValue = std::to_string(params.blackLevel);
        }
        else if (paramName == "autoExposure" && value.is_boolean()) {
            oldValue = params.autoExposure ? "true" : "false";
            params.autoExposure = value.get_boolean();
            newValue = params.autoExposure ? "true" : "false";
        }
        else if (paramName == "autoGain" && value.is_boolean()) {
            oldValue = params.autoGain ? "true" : "false";
            params.autoGain = value.get_boolean();
            newValue = params.autoGain ? "true" : "false";
        }
        else {
            return false; // Unknown parameter
        }
        
        // Log the change
        logParameterChange(serialNumber, paramName, oldValue, newValue, "api");
        
        // Notify callback if registered
        if (parameterChangeCallback_) {
            parameterChangeCallback_(serialNumber, params);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Failed to set parameter " << paramName << ": " << e.what() << std::endl;
        return false;
    }
}

void CameraConfigManager::setDefaultParameters(const CameraParameters& params) {
    std::lock_guard<std::mutex> lock(configMutex_);
    defaultParameters_ = params;
}

CameraParameters CameraConfigManager::getDefaultParameters() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return defaultParameters_;
}

bool CameraConfigManager::validateParameter(const std::string& paramName, const simple_json::JsonValue& value) const {
    if (paramName == "exposureTime" && value.is_number()) {
        int exposure = value.get_int();
        return exposure >= 500 && exposure <= 100000; // From CCF limits
    }
    else if (paramName == "gain" && value.is_number()) {
        double gain = value.get_number();
        return gain >= 1.0 && gain <= 4.0; // From CCF limits
    }
    else if (paramName == "blackLevel" && value.is_number()) {
        int blackLevel = value.get_int();
        return blackLevel >= 0 && blackLevel <= 255;
    }
    else if ((paramName == "autoExposure" || paramName == "autoGain") && value.is_boolean()) {
        return true;
    }
    
    return false;
}

void CameraConfigManager::registerParameterChangeCallback(ParameterChangeCallback callback) {
    std::lock_guard<std::mutex> lock(configMutex_);
    parameterChangeCallback_ = callback;
}

bool CameraConfigManager::applyParametersToCamera(const std::string& serialNumber, void* sapAcqDevice) const {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    auto it = cameras_.find(serialNumber);
    if (it == cameras_.end() || !sapAcqDevice) {
        return false;
    }
    
    SapAcqDevice* device = static_cast<SapAcqDevice*>(sapAcqDevice);
    const auto& params = it->second.parameters;
    
    try {
        if (!quietMode_) {
            std::cout << "Applying parameters to camera " << serialNumber << std::endl;
        }
        
        // Check if we should use auto or manual mode
        if (params.autoExposure && params.autoGain) {
            // Auto exposure mode (like CamExpert)
            if (!quietMode_) {
                std::cout << "  Setting up auto exposure like CamExpert..." << std::endl;
            }
            
            // Set gain selector
            device->SetFeatureValue("GainSelector", "SensorAll");
            
            // Enable auto exposure and auto gain (like in CCF file)
            device->SetFeatureValue("ExposureAuto", "Continuous");
            device->SetFeatureValue("GainAuto", "Continuous");
            
            // Set auto exposure limits based on our target values
            device->SetFeatureValue("exposureAutoMinValue", 500);
            int maxExposure = (params.exposureTime > 30000) ? params.exposureTime : 30000;
            device->SetFeatureValue("exposureAutoMaxValue", maxExposure);
            device->SetFeatureValue("gainAutoMinValue", 1.0);
            double maxGain = (params.gain > 4.0) ? params.gain : 4.0;
            device->SetFeatureValue("gainAutoMaxValue", maxGain);
            
            // Set auto brightness target (from CCF)
            device->SetFeatureValue("autoBrightnessTarget", 2048);
            device->SetFeatureValue("autoBrightnessTargetRangeVariation", 256);
            
            if (!quietMode_) {
                std::cout << "  ✓ Enabled auto exposure (max: " << maxExposure << "μs)" << std::endl;
                std::cout << "  ✓ Enabled auto gain (max: " << maxGain << ")" << std::endl;
                std::cout << "  ✓ Auto brightness target: 2048" << std::endl;
            }
            
            // OPTIMIZED: Reduced auto-adjust time for parallel init
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
        } else {
            // Manual mode
            if (!quietMode_) {
                std::cout << "  Setting manual exposure values..." << std::endl;
            }
            
            // Disable auto modes first
            device->SetFeatureValue("ExposureAuto", "Off");
            device->SetFeatureValue("GainAuto", "Off");
            
            // OPTIMIZED: Minimal wait for auto mode disable during parallel init
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            // Set gain selector
            device->SetFeatureValue("GainSelector", "SensorAll");
            
            // Apply exposure time
            bool exposureSet = false;
            if (device->SetFeatureValue("ExposureTime", static_cast<double>(params.exposureTime))) {
                if (!quietMode_) {
                    std::cout << "  ✓ Set ExposureTime to " << params.exposureTime << "μs" << std::endl;
                }
                exposureSet = true;
            }
            else if (device->SetFeatureValue("ExposureTimeAbs", static_cast<double>(params.exposureTime))) {
                if (!quietMode_) {
                    std::cout << "  ✓ Set ExposureTimeAbs to " << params.exposureTime << "μs" << std::endl;
                }
                exposureSet = true;
            }
            else if (device->SetFeatureValue("ExposureTimeRaw", params.exposureTime)) {
                if (!quietMode_) {
                    std::cout << "  ✓ Set ExposureTimeRaw to " << params.exposureTime << "μs" << std::endl;
                }
                exposureSet = true;
            }
            
            if (!exposureSet && !quietMode_) {
                std::cerr << "  WARNING: Failed to set exposure time" << std::endl;
            }
            
            // Apply gain
            if (device->SetFeatureValue("Gain", params.gain)) {
                if (!quietMode_) {
                    std::cout << "  ✓ Set Gain to " << params.gain << std::endl;
                }
            } else if (!quietMode_) {
                std::cerr << "  WARNING: Failed to set Gain to " << params.gain << std::endl;
            }
            
            // Apply black level if different from default
            if (params.blackLevel != 40) {
                device->SetFeatureValue("BlackLevelSelector", "AnalogAll");
                if (device->SetFeatureValue("BlackLevelRaw", params.blackLevel)) {
                    if (!quietMode_) {
                        std::cout << "  ✓ Set BlackLevel to " << params.blackLevel << std::endl;
                    }
                }
            }
            
            // OPTIMIZED: Minimal settling time for parallel init
            // Settings will be validated during first capture if needed
        }
        
        // STREAMLINED: Skip parameter validation during fast parallel init
        // Read back actual values only for final verification if needed
        char exposureStr[256] = "";
        char gainStr[256] = "";
        
        if (device->GetFeatureValue("ExposureTime", exposureStr, sizeof(exposureStr)) ||
            device->GetFeatureValue("ExposureTimeAbs", exposureStr, sizeof(exposureStr))) {
            double actualExposure = std::atof(exposureStr);
            if (!quietMode_) {
                std::cout << "  📊 Actual exposure: " << actualExposure << "μs" << std::endl;
            }
        }
        
        if (device->GetFeatureValue("Gain", gainStr, sizeof(gainStr))) {
            double actualGain = std::atof(gainStr);
            if (!quietMode_) {
                std::cout << "  📊 Actual gain: " << actualGain << std::endl;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        if (!quietMode_) {
            std::cerr << "ERROR: Failed to apply parameters to camera " << serialNumber << ": " << e.what() << std::endl;
        }
        return false;
    }
}

simple_json::JsonValue CameraConfigManager::getCameraListJson() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    simple_json::JsonArray cameraArray;
    
    for (const auto& pair : cameras_) {
        const auto& camera = pair.second;
        
        simple_json::JsonObject cameraObj;
        cameraObj["serialNumber"] = simple_json::JsonValue(camera.serialNumber);
        cameraObj["position"] = simple_json::JsonValue(camera.position);
        cameraObj["connected"] = simple_json::JsonValue(camera.isConnected);
        cameraObj["serverName"] = simple_json::JsonValue(camera.serverName);
        cameraObj["modelName"] = simple_json::JsonValue(camera.modelName);
        
        // Add parameters
        simple_json::JsonObject paramsObj;
        paramsObj["exposureTime"] = simple_json::JsonValue(camera.parameters.exposureTime);
        paramsObj["gain"] = simple_json::JsonValue(camera.parameters.gain);
        paramsObj["blackLevel"] = simple_json::JsonValue(camera.parameters.blackLevel);
        paramsObj["autoExposure"] = simple_json::JsonValue(camera.parameters.autoExposure);
        paramsObj["autoGain"] = simple_json::JsonValue(camera.parameters.autoGain);
        
        cameraObj["parameters"] = simple_json::JsonValue(paramsObj);
        cameraArray.push_back(simple_json::JsonValue(cameraObj));
    }
    
    simple_json::JsonObject result;
    result["cameras"] = simple_json::JsonValue(cameraArray);
    result["count"] = simple_json::JsonValue(static_cast<int>(cameras_.size()));
    
    return simple_json::JsonValue(result);
}

// Helper methods
void CameraConfigManager::logParameterChange(const std::string& serialNumber, const std::string& paramName, 
                                            const std::string& oldValue, const std::string& newValue, const std::string& source) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    
    ParameterChange change;
    change.timestamp = ss.str();
    change.serialNumber = serialNumber;
    change.parameterName = paramName;
    change.oldValue = oldValue;
    change.newValue = newValue;
    change.source = source;
    
    parameterHistory_.push_back(change);
    
    // Keep only last 100 changes
    if (parameterHistory_.size() > 100) {
        parameterHistory_.erase(parameterHistory_.begin());
    }
    
    std::cout << "Parameter change logged: " << serialNumber << "." << paramName 
              << " " << oldValue << " → " << newValue << " (source: " << source << ")" << std::endl;
}

// Parameter utility implementations
namespace ParameterUtils {
    bool isExposureValid(int exposureTime) {
        return exposureTime >= 500 && exposureTime <= 100000;
    }
    
    bool isGainValid(double gain) {
        return gain >= 1.0 && gain <= 4.0;
    }
    
    bool isBlackLevelValid(int blackLevel) {
        return blackLevel >= 0 && blackLevel <= 255;
    }
}

// Camera capture implementations
bool CameraConfigManager::captureFromCamera(const std::string& serialNumber, const std::string& outputDir, const std::string& format) {
    try {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        auto it = cameras_.find(serialNumber);
        if (it == cameras_.end() || !it->second.isConnected) {
            std::cerr << "Camera not found or not connected: " << serialNumber << std::endl;
            return false;
        }
        
        // Create output directory if it doesn't exist
#ifdef _WIN32
        _mkdir(outputDir.c_str());
#else
        mkdir(outputDir.c_str(), 0777);
#endif
        
        // Generate timestamp for filename
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d_%H-%M-%S");
        std::string timestamp = ss.str();
        
        // Build filename
        std::string filename = outputDir + "/" + it->second.modelName + "_" + 
                              std::to_string(it->second.position) + "_" + 
                              timestamp + "." + format;
        
        std::cout << " Capturing from camera " << serialNumber << " to " << filename << std::endl;
        
        // Check if we have a real camera handle registered
        if (hasRealCameraHandle(serialNumber)) {
            auto handleIt = cameraHandles_.find(serialNumber);
            if (!handleIt->second.isAvailable || !handleIt->second.acqDevice || !handleIt->second.buffer || !handleIt->second.transfer) {
                std::cerr << "Camera handle not properly initialized for " << serialNumber << std::endl;
                return false;
            }
            
            // Use the real Sapera camera handle to capture
            SapAcqDevice* acqDevice = handleIt->second.acqDevice;
            SapBuffer* buffer = handleIt->second.buffer;
            SapAcqDeviceToBuf* transfer = handleIt->second.transfer;
            
            // Apply current parameters to camera
            applyParametersToCamera(serialNumber, acqDevice);
            
            // Perform capture using Sapera SDK - use the transfer object
            if (!transfer->Grab()) {
                std::cerr << "Failed to grab image from camera " << serialNumber << std::endl;
                return false;
            }
            
            // Wait for transfer to complete
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Save buffer to file
            if (!buffer->Save(filename.c_str(), ("-format " + format).c_str())) {
                std::cerr << "Failed to save image to " << filename << std::endl;
                return false;
            }
            
            std::cout << " Real Sapera capture completed successfully for camera " << serialNumber << std::endl;
            std::cout << "   Image saved: " << filename << std::endl;
            
        } else {
            // Fallback to placeholder (camera handle not registered)
            std::cout << "   ⚠️ Camera handle not registered, using placeholder..." << std::endl;
            
            std::ofstream imageFile(filename);
            if (!imageFile) {
                std::cerr << "Failed to create output file: " << filename << std::endl;
                return false;
            }
            
            // Write a simple placeholder
            imageFile << "# Sapera Camera Capture Placeholder\n";
            imageFile << "# Camera: " << serialNumber << "\n";
            imageFile << "# Position: " << it->second.position << "\n";
            imageFile << "# Timestamp: " << timestamp << "\n";
            imageFile << "# Exposure: " << it->second.parameters.exposureTime << " μs\n";
            imageFile << "# Gain: " << it->second.parameters.gain << "\n";
            imageFile << "# Format: " << format << "\n";
            imageFile << "# Status: PLACEHOLDER - Camera handle not registered with web system\n";
            imageFile.close();
            
            std::cout << "✅ Placeholder capture completed for camera " << serialNumber << std::endl;
            std::cout << "   📄 File saved: " << filename << std::endl;
            std::cout << "   💡 Note: Register camera handles for real Sapera capture" << std::endl;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Capture failed for camera " << serialNumber << ": " << e.what() << std::endl;
        return false;
    }
}

bool CameraConfigManager::captureFromAllCameras(const std::string& outputDir, const std::string& format) {
    auto connectedCameras = getConnectedCameras();
    
    if (connectedCameras.empty()) {
        std::cerr << "No connected cameras available for capture" << std::endl;
        return false;
    }
    
    std::cout << "📸 Starting batch capture from " << connectedCameras.size() << " cameras..." << std::endl;
    
    int successCount = 0;
    for (const auto& camera : connectedCameras) {
        if (captureFromCamera(camera.serialNumber, outputDir, format)) {
            successCount++;
        }
        // Small delay between captures
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "📊 Batch capture complete: " << successCount << "/" << connectedCameras.size() << " successful" << std::endl;
    return successCount > 0;
}

// Camera handle registration methods
void CameraConfigManager::registerCameraHandle(const std::string& serialNumber, 
                                              SapAcqDevice* acqDevice, 
                                              SapAcqDeviceToBuf* transfer, 
                                              SapBuffer* buffer) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    WebCameraHandle handle;
    handle.acqDevice = acqDevice;
    handle.transfer = transfer;
    handle.buffer = buffer;
    handle.isAvailable = true;
    
    cameraHandles_[serialNumber] = handle;
    
    if (!quietMode_) {
        std::cout << "✅ Registered camera handle for " << serialNumber << std::endl;
    }
}

void CameraConfigManager::unregisterCameraHandle(const std::string& serialNumber) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    auto it = cameraHandles_.find(serialNumber);
    if (it != cameraHandles_.end()) {
        cameraHandles_.erase(it);
        if (!quietMode_) {
            std::cout << "🔌 Unregistered camera handle for " << serialNumber << std::endl;
        }
    }
}

bool CameraConfigManager::hasRealCameraHandle(const std::string& serialNumber) const {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    auto it = cameraHandles_.find(serialNumber);
    return it != cameraHandles_.end() && it->second.isAvailable;
} 