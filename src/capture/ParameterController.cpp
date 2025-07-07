#include "ParameterController.hpp"
#include "NeuralCaptureSystem.hpp"

ParameterController::ParameterController() {
    // Initialize parameter definitions for Nano-C4020 cameras
    parameterDefinitions_ = {
        {"ExposureTime", {"ExposureTime", "Exposure time", 1000, 100000, 40000, "μs", false}},
        {"Gain", {"Gain", "Analog gain", 1.0, 10.0, 1.0, "dB", false}},
        {"BlackLevel", {"BlackLevel", "Black level offset", 0, 255, 0, "counts", false}},
        {"Gamma", {"Gamma", "Gamma correction", 0.1, 3.0, 1.0, "", false}},
        {"OffsetX", {"OffsetX", "Horizontal offset", 0, 1024, 0, "pixels", false}},
        {"OffsetY", {"OffsetY", "Vertical offset", 0, 768, 0, "pixels", false}},
        {"Width", {"Width", "Image width", 64, 4112, 4112, "pixels", false}},
        {"Height", {"Height", "Image height", 64, 3008, 3008, "pixels", false}},
        {"PixelFormat", {"PixelFormat", "Pixel format", 0, 0, 0, "", false}},
        {"TriggerMode", {"TriggerMode", "Trigger mode", 0, 0, 0, "", false}},
        {"TriggerSource", {"TriggerSource", "Trigger source", 0, 0, 0, "", false}},
        {"AcquisitionMode", {"AcquisitionMode", "Acquisition mode", 0, 0, 0, "", false}},
        {"DeviceTemperature", {"DeviceTemperature", "Device temperature", -40, 85, 25, "°C", true}},
        {"DeviceSerialNumber", {"DeviceSerialNumber", "Device serial number", 0, 0, 0, "", true}},
        {"DeviceModelName", {"DeviceModelName", "Device model name", 0, 0, 0, "", true}},
        {"DeviceVendorName", {"DeviceVendorName", "Device vendor name", 0, 0, 0, "", true}},
        {"DeviceVersion", {"DeviceVersion", "Device version", 0, 0, 0, "", true}},
        {"SensorWidth", {"SensorWidth", "Sensor width", 0, 0, 0, "pixels", true}},
        {"SensorHeight", {"SensorHeight", "Sensor height", 0, 0, 0, "pixels", true}},
        {"AcquisitionFrameRate", {"AcquisitionFrameRate", "Frame rate", 0.1, 30.0, 1.0, "fps", false}},
        {"WhiteBalanceRed", {"WhiteBalanceRed", "White balance red", 0.1, 4.0, 1.0, "", false}},
        {"WhiteBalanceGreen", {"WhiteBalanceGreen", "White balance green", 0.1, 4.0, 1.0, "", false}},
        {"WhiteBalanceBlue", {"WhiteBalanceBlue", "White balance blue", 0.1, 4.0, 1.0, "", false}}
    };
}

void ParameterController::setCameras(std::map<std::string, ConnectedCamera>& cameras) {
    cameras_.clear();
    for (auto& [id, camera] : cameras) {
        cameras_[id] = &camera;
    }
}

bool ParameterController::getParameter(const std::string& cameraId, const std::string& paramName, std::string& value) {
    auto it = cameras_.find(cameraId);
    if (it == cameras_.end() || !it->second->acqDevice) {
        return false;
    }
    
    try {
        char buffer[512];
        if (it->second->acqDevice->GetFeatureValue(paramName.c_str(), buffer, sizeof(buffer))) {
            value = buffer;
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception getting parameter " << paramName << ": " << e.what() << std::endl;
    }
    return false;
}

bool ParameterController::setParameter(const std::string& cameraId, const std::string& paramName, const std::string& value) {
    auto it = cameras_.find(cameraId);
    if (it == cameras_.end() || !it->second->acqDevice) {
        return false;
    }
    
    try {
        if (it->second->acqDevice->SetFeatureValue(paramName.c_str(), value.c_str())) {
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception setting parameter " << paramName << ": " << e.what() << std::endl;
    }
    return false;
}

bool ParameterController::setParameterAll(const std::string& paramName, const std::string& value) {
    bool success = true;
    int successCount = 0;
    
    for (auto& [cameraId, camera] : cameras_) {
        if (setParameter(cameraId, paramName, value)) {
            successCount++;
        } else {
            success = false;
        }
    }
    
    std::cout << "📝 Parameter '" << paramName << "' set to '" << value << "' on " 
              << successCount << "/" << cameras_.size() << " cameras" << std::endl;
    return success;
}

bool ParameterController::getParameterInfo(const std::string& paramName, ParameterInfo& info) {
    auto it = parameterDefinitions_.find(paramName);
    if (it != parameterDefinitions_.end()) {
        info = it->second;
        return true;
    }
    return false;
}

void ParameterController::listParameters() {
    std::cout << "\n=== Available Camera Parameters ===" << std::endl;
    std::cout << "Parameter Name           | Description                | Range/Options        | Unit | RW" << std::endl;
    std::cout << "-------------------------|----------------------------|---------------------|------|----" << std::endl;
    
    for (const auto& [name, info] : parameterDefinitions_) {
        std::string range;
        if (info.name == "PixelFormat" || info.name == "TriggerMode" || 
            info.name == "TriggerSource" || info.name == "AcquisitionMode") {
            range = "Enum";
        } else if (info.isReadOnly) {
            range = "Read-only";
        } else {
            range = std::to_string(info.minValue) + " - " + std::to_string(info.maxValue);
        }
        
        std::cout << std::setw(24) << std::left << info.name << " | "
                  << std::setw(26) << std::left << info.description << " | "
                  << std::setw(19) << std::left << range << " | "
                  << std::setw(4) << std::left << info.unit << " | "
                  << (info.isReadOnly ? "R" : "RW") << std::endl;
    }
    std::cout << std::endl;
}

void ParameterController::getParameterStatus(const std::string& paramName) {
    std::cout << "\n=== Parameter Status: " << paramName << " ===" << std::endl;
    
    ParameterInfo info;
    if (getParameterInfo(paramName, info)) {
        std::cout << "Description: " << info.description << std::endl;
        if (!info.isReadOnly) {
            std::cout << "Range: " << info.minValue << " - " << info.maxValue << " " << info.unit << std::endl;
        }
        std::cout << std::endl;
    }
    
    // Get current values from all cameras
    for (const auto& [cameraId, camera] : cameras_) {
        std::string value;
        if (getParameter(cameraId, paramName, value)) {
            std::cout << "📸 " << camera->info.name << ": " << value << std::endl;
        } else {
            std::cout << "📸 " << camera->info.name << ": ❌ Failed to read" << std::endl;
        }
    }
    std::cout << std::endl;
}

bool ParameterController::setROI(int offsetX, int offsetY, int width, int height) {
    std::cout << "📐 Setting ROI: " << offsetX << "," << offsetY << " " << width << "x" << height << std::endl;
    
    bool success = true;
    success &= setParameterAll("OffsetX", std::to_string(offsetX));
    success &= setParameterAll("OffsetY", std::to_string(offsetY));
    success &= setParameterAll("Width", std::to_string(width));
    success &= setParameterAll("Height", std::to_string(height));
    
    if (success) {
        std::cout << "✅ ROI set successfully on all cameras" << std::endl;
    } else {
        std::cout << "⚠️ ROI partially applied. Some cameras may have failed." << std::endl;
    }
    
    return success;
}

bool ParameterController::setWhiteBalance(double red, double green, double blue) {
    std::cout << "🎨 Setting white balance: R=" << red << " G=" << green << " B=" << blue << std::endl;
    
    bool success = true;
    success &= setParameterAll("WhiteBalanceRed", std::to_string(red));
    success &= setParameterAll("WhiteBalanceGreen", std::to_string(green));
    success &= setParameterAll("WhiteBalanceBlue", std::to_string(blue));
    
    if (success) {
        std::cout << "✅ White balance set successfully on all cameras" << std::endl;
    } else {
        std::cout << "⚠️ White balance partially applied. Some cameras may have failed." << std::endl;
    }
    
    return success;
}

void ParameterController::showCameraInfo() {
    std::cout << "\n=== Camera Hardware Information ===" << std::endl;
    
    for (const auto& [cameraId, camera] : cameras_) {
        std::cout << "\n📸 " << camera->info.name << " (" << camera->info.serialNumber << ")" << std::endl;
        std::cout << "   Server: " << camera->info.serverName << std::endl;
        std::cout << "   Device: " << camera->info.deviceName << std::endl;
        
        // Get hardware info
        std::string value;
        if (getParameter(cameraId, "DeviceModelName", value)) {
            std::cout << "   Model: " << value << std::endl;
        }
        if (getParameter(cameraId, "DeviceVendorName", value)) {
            std::cout << "   Vendor: " << value << std::endl;
        }
        if (getParameter(cameraId, "DeviceVersion", value)) {
            std::cout << "   Version: " << value << std::endl;
        }
        if (getParameter(cameraId, "SensorWidth", value)) {
            std::cout << "   Sensor: " << value;
            if (getParameter(cameraId, "SensorHeight", value)) {
                std::cout << " x " << value;
            }
            std::cout << " pixels" << std::endl;
        }
        if (getParameter(cameraId, "DeviceTemperature", value)) {
            std::cout << "   Temperature: " << value << "°C" << std::endl;
        }
    }
    std::cout << std::endl;
}

void ParameterController::showCurrentSettings() {
    std::cout << "\n=== Current Camera Settings ===" << std::endl;
    
    // Key parameters to show
    std::vector<std::string> keyParams = {
        "ExposureTime", "Gain", "Gamma", "OffsetX", "OffsetY", "Width", "Height",
        "WhiteBalanceRed", "WhiteBalanceGreen", "WhiteBalanceBlue"
    };
    
    for (const std::string& param : keyParams) {
        std::cout << "\n📊 " << param << ":" << std::endl;
        
        for (const auto& [cameraId, camera] : cameras_) {
            std::string value;
            if (getParameter(cameraId, param, value)) {
                
                // Get unit
                std::string unit;
                ParameterInfo info;
                if (getParameterInfo(param, info)) {
                    unit = info.unit;
                }
                
                std::cout << "   " << camera->info.name << ": " << value << " " << unit << std::endl;
            } else {
                std::cout << "   " << camera->info.name << ": ❌ Failed to read" << std::endl;
            }
        }
    }
    std::cout << std::endl;
} 