#pragma once

#include <map>
#include <string>
#include <iostream>
#include <iomanip>
#include "SapClassBasic.h"

// Forward declaration
struct ConnectedCamera;

class ParameterController {
private:
    std::map<std::string, ConnectedCamera*> cameras_;
    
    // Common parameter definitions
    struct ParameterInfo {
        std::string name;
        std::string description;
        double minValue;
        double maxValue;
        double defaultValue;
        std::string unit;
        bool isReadOnly;
    };
    
    // Parameter definitions for Nano-C4020 cameras
    std::map<std::string, ParameterInfo> parameterDefinitions_;
    
public:
    ParameterController();
    
    void setCameras(std::map<std::string, ConnectedCamera>& cameras);
    
    // Get parameter value from a specific camera
    bool getParameter(const std::string& cameraId, const std::string& paramName, std::string& value);
    
    // Set parameter value on a specific camera
    bool setParameter(const std::string& cameraId, const std::string& paramName, const std::string& value);
    
    // Set parameter on all cameras
    bool setParameterAll(const std::string& paramName, const std::string& value);
    
    // Get parameter info
    bool getParameterInfo(const std::string& paramName, ParameterInfo& info);
    
    // List all available parameters
    void listParameters();
    
    // Get parameter status from all cameras
    void getParameterStatus(const std::string& paramName);
    
    // Convenience methods
    bool setROI(int offsetX, int offsetY, int width, int height);
    bool setWhiteBalance(double red, double green, double blue);
    void showCameraInfo();
    void showCurrentSettings();
}; 