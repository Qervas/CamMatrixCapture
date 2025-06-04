#include "CameraAPI.hpp"
#include <sstream>
#include <iostream>
#include <chrono>
#include <ctime>

// APIResponse implementations
APIResponse APIResponse::success(const simple_json::JsonValue& data) {
    APIResponse response;
    response.statusCode = 200;
    response.contentType = "application/json";
    response.body = data.serialize();
    return response;
}

APIResponse APIResponse::error(int code, const std::string& message) {
    APIResponse response;
    response.statusCode = code;
    response.contentType = "application/json";
    
    simple_json::JsonObject errorObj;
    errorObj["error"] = simple_json::JsonValue(message);
    errorObj["code"] = simple_json::JsonValue(code);
    response.body = simple_json::JsonValue(errorObj).serialize();
    
    return response;
}

// CameraAPI implementation
CameraAPI::CameraAPI(CameraConfigManager& configManager) 
    : configManager_(configManager) {
    registerRoutes();
}

void CameraAPI::registerRoutes() {
    // Camera discovery and status
    routes_[{"GET", "/api/cameras"}] = [this](const APIRequest& req) { return getCameraList(req); };
    routes_[{"GET", "/api/cameras/*"}] = [this](const APIRequest& req) { return getCameraStatus(req); };
    routes_[{"POST", "/api/cameras/refresh"}] = [this](const APIRequest& req) { return refreshCameras(req); };
    
    // Parameter management
    routes_[{"GET", "/api/cameras/*/parameters"}] = [this](const APIRequest& req) { return getParameters(req); };
    routes_[{"PUT", "/api/cameras/*/parameters/*"}] = [this](const APIRequest& req) { return setParameter(req); };
    routes_[{"PUT", "/api/cameras/*/parameters"}] = [this](const APIRequest& req) { return setParameters(req); };
    
    // Capture operations
    routes_[{"POST", "/api/cameras/*/capture"}] = [this](const APIRequest& req) { return captureImage(req); };
    routes_[{"POST", "/api/capture"}] = [this](const APIRequest& req) { return captureAll(req); };
    
    // Defaults and presets
    routes_[{"GET", "/api/defaults"}] = [this](const APIRequest& req) { return getDefaults(req); };
    routes_[{"PUT", "/api/defaults"}] = [this](const APIRequest& req) { return setDefaults(req); };
    routes_[{"GET", "/api/presets"}] = [this](const APIRequest& req) { return getPresets(req); };
}

APIResponse CameraAPI::handleRequest(const APIRequest& request) {
    std::cout << "API Request: " << request.method << " " << request.path << std::endl;
    
    // Find matching route
    for (const auto& route : routes_) {
        const auto& routeMethod = route.first.first;
        const auto& routePattern = route.first.second;
        
        if (request.method == routeMethod && matchRoute(routePattern, request.path)) {
            try {
                return route.second(request);
            } catch (const std::exception& e) {
                return APIResponse::error(500, "Internal server error: " + std::string(e.what()));
            }
        }
    }
    
    return APIResponse::error(404, "Endpoint not found");
}

APIResponse CameraAPI::getCameraList(const APIRequest& request) {
    auto cameras = configManager_.getCameraListJson();
    return APIResponse::success(cameras);
}

APIResponse CameraAPI::getCameraStatus(const APIRequest& request) {
    std::string serial = extractSerialFromPath(request.path);
    if (serial.empty()) {
        return APIResponse::error(400, "Invalid camera serial number");
    }
    
    auto camera = configManager_.getCamera(serial);
    if (!camera) {
        return APIResponse::error(404, "Camera not found: " + serial);
    }
    
    simple_json::JsonObject result;
    result["serialNumber"] = simple_json::JsonValue(camera->serialNumber);
    result["position"] = simple_json::JsonValue(camera->position);
    result["connected"] = simple_json::JsonValue(camera->isConnected);
    result["serverName"] = simple_json::JsonValue(camera->serverName);
    result["modelName"] = simple_json::JsonValue(camera->modelName);
    
    // Add current parameters
    simple_json::JsonObject paramsObj;
    paramsObj["exposureTime"] = simple_json::JsonValue(camera->parameters.exposureTime);
    paramsObj["gain"] = simple_json::JsonValue(camera->parameters.gain);
    paramsObj["blackLevel"] = simple_json::JsonValue(camera->parameters.blackLevel);
    paramsObj["autoExposure"] = simple_json::JsonValue(camera->parameters.autoExposure);
    paramsObj["autoGain"] = simple_json::JsonValue(camera->parameters.autoGain);
    
    result["parameters"] = simple_json::JsonValue(paramsObj);
    
    return APIResponse::success(simple_json::JsonValue(result));
}

APIResponse CameraAPI::getParameters(const APIRequest& request) {
    std::string serial = extractSerialFromPath(request.path);
    if (serial.empty()) {
        return APIResponse::error(400, "Invalid camera serial number");
    }
    
    auto params = configManager_.getParameters(serial);
    
    simple_json::JsonObject result;
    result["exposureTime"] = simple_json::JsonValue(params.exposureTime);
    result["gain"] = simple_json::JsonValue(params.gain);
    result["blackLevel"] = simple_json::JsonValue(params.blackLevel);
    result["autoExposure"] = simple_json::JsonValue(params.autoExposure);
    result["autoGain"] = simple_json::JsonValue(params.autoGain);
    result["pixelFormat"] = simple_json::JsonValue(params.pixelFormat);
    
    // Add limits for validation
    simple_json::JsonObject limits;
    limits["exposureTime"] = simple_json::JsonObject{
        {"min", simple_json::JsonValue(params.limits.minExposure)},
        {"max", simple_json::JsonValue(params.limits.maxExposure)}
    };
    limits["gain"] = simple_json::JsonObject{
        {"min", simple_json::JsonValue(params.limits.minGain)},
        {"max", simple_json::JsonValue(params.limits.maxGain)}
    };
    
    result["limits"] = simple_json::JsonValue(limits);
    
    return APIResponse::success(simple_json::JsonValue(result));
}

APIResponse CameraAPI::setParameter(const APIRequest& request) {
    std::string serial = extractSerialFromPath(request.path);
    std::string paramName = extractParameterFromPath(request.path);
    
    if (serial.empty() || paramName.empty()) {
        return APIResponse::error(400, "Invalid camera serial number or parameter name");
    }
    
    try {
        auto json = parseRequestBody(request.body);
        auto valueField = json["value"];
        
        if (!valueField.is_valid()) {
            return APIResponse::error(400, "Missing 'value' field in request body");
        }
        
        // Validate parameter
        if (!configManager_.validateParameter(paramName, valueField)) {
            return APIResponse::error(400, "Invalid parameter value for " + paramName);
        }
        
        // Set the parameter
        if (!configManager_.setParameter(serial, paramName, valueField)) {
            return APIResponse::error(500, "Failed to set parameter " + paramName);
        }
        
        // Return updated parameters
        auto updatedParams = configManager_.getParameters(serial);
        
        simple_json::JsonObject result;
        result["success"] = simple_json::JsonValue(true);
        result["serialNumber"] = simple_json::JsonValue(serial);
        result["parameter"] = simple_json::JsonValue(paramName);
        result["newValue"] = valueField;
        
        return APIResponse::success(simple_json::JsonValue(result));
        
    } catch (const std::exception& e) {
        return APIResponse::error(400, "Invalid JSON in request body: " + std::string(e.what()));
    }
}

APIResponse CameraAPI::setParameters(const APIRequest& request) {
    std::string serial = extractSerialFromPath(request.path);
    if (serial.empty()) {
        return APIResponse::error(400, "Invalid camera serial number");
    }
    
    try {
        auto json = parseRequestBody(request.body);
        
        CameraParameters params = configManager_.getParameters(serial);
        
        // Update parameters from JSON
        if (json["exposureTime"].is_number()) {
            params.exposureTime = json["exposureTime"].get_int();
        }
        if (json["gain"].is_number()) {
            params.gain = json["gain"].get_number();
        }
        if (json["blackLevel"].is_number()) {
            params.blackLevel = json["blackLevel"].get_int();
        }
        if (json["autoExposure"].is_boolean()) {
            params.autoExposure = json["autoExposure"].get_boolean();
        }
        if (json["autoGain"].is_boolean()) {
            params.autoGain = json["autoGain"].get_boolean();
        }
        
        // Set all parameters
        if (!configManager_.setParameters(serial, params)) {
            return APIResponse::error(500, "Failed to set parameters");
        }
        
        simple_json::JsonObject result;
        result["success"] = simple_json::JsonValue(true);
        result["serialNumber"] = simple_json::JsonValue(serial);
        result["updated"] = simple_json::JsonValue(true);
        
        return APIResponse::success(simple_json::JsonValue(result));
        
    } catch (const std::exception& e) {
        return APIResponse::error(400, "Invalid JSON in request body: " + std::string(e.what()));
    }
}

APIResponse CameraAPI::captureImage(const APIRequest& request) {
    std::string serial = extractSerialFromPath(request.path);
    if (serial.empty()) {
        return APIResponse::error(400, "Invalid camera serial number");
    }
    
    // Parse request body for options
    simple_json::JsonValue options;
    std::string outputDir = "captured_images";
    std::string format = "tiff";
    
    try {
        if (!request.body.empty()) {
            options = parseRequestBody(request.body);
            std::cout << "📝 Individual capture request body: " << request.body << std::endl;
        }
    } catch (const std::exception&) {
        // Ignore invalid JSON, use defaults
    }
    
    if (options.is_object()) {
        if (options["outputDir"].is_string()) {
            std::string requestedDir = options["outputDir"].get_string();
            if (!requestedDir.empty()) {
                outputDir = requestedDir;
            }
        }
        if (options["format"].is_string()) {
            format = options["format"].get_string();
            if (format != "tiff" && format != "png" && format != "jpg") {
                format = "tiff";
            }
        }
    }
    
    std::cout << "📸 Individual capture requested for camera: " << serial << std::endl;
    std::cout << "📁 Output directory: " << outputDir << std::endl;
    
    // Perform capture for ONLY this specific camera
    bool success = configManager_.captureFromCamera(serial, outputDir, format);
    
    // Build response
    simple_json::JsonObject result;
    result["success"] = simple_json::JsonValue(success);
    result["serialNumber"] = simple_json::JsonValue(serial);
    result["outputDirectory"] = simple_json::JsonValue(outputDir);
    result["format"] = simple_json::JsonValue(format);
    
    // Add timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);
    char timestamp[100];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", &tm);
    result["timestamp"] = simple_json::JsonValue(std::string(timestamp));
    
    if (success) {
        // Generate expected filename
        std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &tm);
        auto camera = configManager_.getCamera(serial);
        if (camera) {
            std::string filename = outputDir + "/pos" + std::to_string(camera->position) + 
                                 "_" + serial + "_" + timestamp + "." + format;
            result["filename"] = simple_json::JsonValue(filename);
        }
        
        result["message"] = simple_json::JsonValue("Capture completed successfully for camera " + serial);
        std::cout << "✅ Individual capture successful for camera " << serial << std::endl;
    } else {
        result["message"] = simple_json::JsonValue("Capture failed for camera " + serial);
        std::cout << "❌ Individual capture failed for camera " << serial << std::endl;
    }
    
    return APIResponse::success(simple_json::JsonValue(result));
}

APIResponse CameraAPI::captureAll(const APIRequest& request) {
    auto cameras = configManager_.getConnectedCameras();
    
    if (cameras.empty()) {
        return APIResponse::error(400, "No connected cameras available for capture");
    }

    // Create captured_images directory if it doesn't exist
    std::string outputDir = "captured_images";
    
    // Parse request body for options
    simple_json::JsonValue options;
    try {
        if (!request.body.empty()) {
            options = parseRequestBody(request.body);
            std::cout << "📝 Request body received: " << request.body << std::endl;
        }
    } catch (const std::exception&) {
        // Ignore invalid JSON, use defaults
    }
    
    // Get capture parameters
    int shots = 1;
    std::string format = "tiff";
    
    if (options.is_object()) {
        if (options["shots"].is_number()) {
            shots = options["shots"].get_int();
            if (shots < 1 || shots > 10) shots = 1; // Limit shots
        }
        if (options["format"].is_string()) {
            format = options["format"].get_string();
            if (format != "tiff" && format != "png" && format != "jpg") {
                format = "tiff"; // Default to TIFF
            }
        }
        if (options["outputDir"].is_string()) {
            std::string requestedDir = options["outputDir"].get_string();
            std::cout << "📂 Custom output directory requested: '" << requestedDir << "'" << std::endl;
            if (!requestedDir.empty()) {
                outputDir = requestedDir;
                std::cout << "📁 Using custom output directory: '" << outputDir << "'" << std::endl;
            } else {
                std::cout << "⚠️  Empty output directory provided, using default: '" << outputDir << "'" << std::endl;
            }
        } else {
            std::cout << "📁 No custom output directory specified, using default: '" << outputDir << "'" << std::endl;
        }
    } else {
        std::cout << "📁 No options object found, using default output directory: '" << outputDir << "'" << std::endl;
    }
    
    // Perform actual captures using the camera config manager
    std::vector<std::string> capturedFiles;
    int successCount = 0;
    int failCount = 0;
    
    for (const auto& camera : cameras) {
        try {
            // Try to perform capture using the camera system
            bool success = configManager_.captureFromCamera(camera.serialNumber, outputDir, format);
            if (success) {
                successCount++;
                // Generate filename based on camera position and timestamp
                auto now = std::chrono::system_clock::now();
                auto time_t = std::chrono::system_clock::to_time_t(now);
                auto tm = *std::localtime(&time_t);
                
                char timestamp[100];
                std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &tm);
                
                std::string filename = outputDir + "/pos" + std::to_string(camera.position) + 
                                     "_" + camera.serialNumber + "_" + timestamp + "." + format;
                capturedFiles.push_back(filename);
            } else {
                failCount++;
            }
        } catch (const std::exception&) {
            failCount++;
        }
    }
    
    // Build response
    simple_json::JsonObject result;
    result["success"] = simple_json::JsonValue(successCount > 0);
    result["totalCameras"] = simple_json::JsonValue(static_cast<int>(cameras.size()));
    result["successCount"] = simple_json::JsonValue(successCount);
    result["failCount"] = simple_json::JsonValue(failCount);
    result["shots"] = simple_json::JsonValue(shots);
    result["format"] = simple_json::JsonValue(format);
    result["outputDirectory"] = simple_json::JsonValue(outputDir);
    
    // Add timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);
    char timestamp[100];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", &tm);
    result["timestamp"] = simple_json::JsonValue(std::string(timestamp));
    
    // Build message
    std::string message;
    if (successCount == cameras.size()) {
        message = "All " + std::to_string(successCount) + " cameras captured successfully!";
    } else if (successCount > 0) {
        message = std::to_string(successCount) + "/" + std::to_string(cameras.size()) + " cameras captured successfully";
    } else {
        message = "Capture failed for all cameras";
    }
    result["message"] = simple_json::JsonValue(message);
    
    // Add captured camera details
    simple_json::JsonArray capturedCameras;
    for (size_t i = 0; i < cameras.size() && i < capturedFiles.size(); i++) {
        simple_json::JsonObject cameraObj;
        cameraObj["serialNumber"] = simple_json::JsonValue(cameras[i].serialNumber);
        cameraObj["position"] = simple_json::JsonValue(cameras[i].position);
        cameraObj["filename"] = simple_json::JsonValue(capturedFiles[i]);
        cameraObj["success"] = simple_json::JsonValue(i < successCount);
        capturedCameras.push_back(simple_json::JsonValue(cameraObj));
    }
    result["cameras"] = simple_json::JsonValue(capturedCameras);
    
    std::cout << "✅ Capture complete: " << successCount << "/" << cameras.size() << " cameras successful" << std::endl;
    std::cout << "📁 Images saved to: " << outputDir << "/" << std::endl;
    
    return APIResponse::success(simple_json::JsonValue(result));
}

APIResponse CameraAPI::getDefaults(const APIRequest& request) {
    auto params = configManager_.getDefaultParameters();
    
    simple_json::JsonObject result;
    result["exposureTime"] = simple_json::JsonValue(params.exposureTime);
    result["gain"] = simple_json::JsonValue(params.gain);
    result["blackLevel"] = simple_json::JsonValue(params.blackLevel);
    result["autoExposure"] = simple_json::JsonValue(params.autoExposure);
    result["autoGain"] = simple_json::JsonValue(params.autoGain);
    result["pixelFormat"] = simple_json::JsonValue(params.pixelFormat);
    
    return APIResponse::success(simple_json::JsonValue(result));
}

APIResponse CameraAPI::setDefaults(const APIRequest& request) {
    try {
        auto json = parseRequestBody(request.body);
        
        CameraParameters params = configManager_.getDefaultParameters();
        
        // Update default parameters from JSON
        if (json["exposureTime"].is_number()) {
            params.exposureTime = json["exposureTime"].get_int();
        }
        if (json["gain"].is_number()) {
            params.gain = json["gain"].get_number();
        }
        if (json["blackLevel"].is_number()) {
            params.blackLevel = json["blackLevel"].get_int();
        }
        if (json["autoExposure"].is_boolean()) {
            params.autoExposure = json["autoExposure"].get_boolean();
        }
        if (json["autoGain"].is_boolean()) {
            params.autoGain = json["autoGain"].get_boolean();
        }
        
        configManager_.setDefaultParameters(params);
        
        simple_json::JsonObject result;
        result["success"] = simple_json::JsonValue(true);
        result["message"] = simple_json::JsonValue("Default parameters updated");
        
        return APIResponse::success(simple_json::JsonValue(result));
        
    } catch (const std::exception& e) {
        return APIResponse::error(400, "Invalid JSON in request body: " + std::string(e.what()));
    }
}

APIResponse CameraAPI::getPresets(const APIRequest& request) {
    // Placeholder for presets functionality
    simple_json::JsonObject result;
    result["presets"] = simple_json::JsonArray();
    result["count"] = simple_json::JsonValue(0);
    
    return APIResponse::success(simple_json::JsonValue(result));
}

APIResponse CameraAPI::refreshCameras(const APIRequest& request) {
    // Placeholder for camera refresh functionality
    simple_json::JsonObject result;
    result["success"] = simple_json::JsonValue(true);
    result["message"] = simple_json::JsonValue("Camera refresh initiated");
    result["timestamp"] = simple_json::JsonValue("2024-01-01T12:00:00Z");
    
    return APIResponse::success(simple_json::JsonValue(result));
}

// Helper methods
std::string CameraAPI::extractSerialFromPath(const std::string& path) {
    // Extract serial number from paths like "/api/cameras/S1128470/..."
    size_t camerasPos = path.find("/api/cameras/");
    if (camerasPos == std::string::npos) {
        return "";
    }
    
    size_t serialStart = camerasPos + 13; // Length of "/api/cameras/"
    if (serialStart >= path.length()) {
        return "";
    }
    
    size_t serialEnd = path.find('/', serialStart);
    if (serialEnd == std::string::npos) {
        serialEnd = path.length();
    }
    
    return path.substr(serialStart, serialEnd - serialStart);
}

std::string CameraAPI::extractParameterFromPath(const std::string& path) {
    // Extract parameter name from paths like "/api/cameras/S1128470/parameters/exposureTime"
    size_t parametersPos = path.find("/parameters/");
    if (parametersPos == std::string::npos) {
        return "";
    }
    
    size_t paramStart = parametersPos + 12; // Length of "/parameters/"
    if (paramStart >= path.length()) {
        return "";
    }
    
    size_t paramEnd = path.find('/', paramStart);
    if (paramEnd == std::string::npos) {
        paramEnd = path.length();
    }
    
    return path.substr(paramStart, paramEnd - paramStart);
}

simple_json::JsonValue CameraAPI::parseRequestBody(const std::string& body) {
    if (body.empty()) {
        return simple_json::JsonValue(simple_json::JsonObject());
    }
    
    return simple_json::JsonParser::parse(body);
}

bool CameraAPI::matchRoute(const std::string& pattern, const std::string& path) const {
    // Simple wildcard matching where * matches any path segment
    if (pattern == path) {
        return true;
    }
    
    if (pattern.find('*') == std::string::npos) {
        return false;
    }
    
    // Replace * with regex patterns and do basic matching
    std::string regexPattern = pattern;
    size_t pos = 0;
    while ((pos = regexPattern.find('*', pos)) != std::string::npos) {
        regexPattern.replace(pos, 1, "[^/]+");
        pos += 5; // Length of "[^/]+"
    }
    
    // For simplicity, just check if the path starts with the pattern prefix
    size_t starPos = pattern.find('*');
    if (starPos != std::string::npos) {
        std::string prefix = pattern.substr(0, starPos);
        return path.substr(0, prefix.length()) == prefix;
    }
    
    return false;
} 