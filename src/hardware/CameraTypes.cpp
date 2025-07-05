/**
 * CameraTypes.cpp - Simplified Camera Type Definitions
 * Based on working simple_camera_test.cpp approach
 */

#include "CameraTypes.hpp"
#include <sstream>
#include <iomanip>
#include <iostream>

namespace SaperaCapturePro {

// Simple enum to string conversions
std::string toString(CameraStatus status) {
    switch (status) {
        case CameraStatus::Disconnected: return "Disconnected";
        case CameraStatus::Connected: return "Connected";
        case CameraStatus::Initializing: return "Initializing";
        case CameraStatus::Ready: return "Ready";
        case CameraStatus::Capturing: return "Capturing";
        case CameraStatus::Error: return "Error";
        default: return "Unknown";
    }
}

std::string toString(CameraType type) {
    switch (type) {
        case CameraType::Area: return "Area";
        case CameraType::Line: return "Line";
        case CameraType::Industrial: return "Industrial";
        default: return "Unknown";
    }
}

std::string toString(PixelFormat format) {
    switch (format) {
        case PixelFormat::Mono8: return "Mono8";
        case PixelFormat::Mono10: return "Mono10";
        case PixelFormat::Mono12: return "Mono12";
        case PixelFormat::RGB8: return "RGB8";
        case PixelFormat::BayerRG8: return "BayerRG8";
        default: return "Unknown";
    }
}

std::string toString(TriggerMode mode) {
    switch (mode) {
        case TriggerMode::Off: return "Off";
        case TriggerMode::Software: return "Software";
        case TriggerMode::Hardware: return "Hardware";
        default: return "Unknown";
    }
}

// CameraParameter implementation
bool CameraParameter::setValue(const std::string& value) {
    try {
        switch (type) {
            case Type::Integer:
                this->value = std::stoi(value);
                return true;
            case Type::Float:
                this->value = std::stod(value);
                return true;
            case Type::Boolean:
                this->value = (value == "true" || value == "1");
                return true;
            case Type::String:
                this->value = value;
                return true;
            case Type::Enumeration:
                this->value = value;
                return true;
        }
    } catch (const std::exception& e) {
        std::cout << "Error setting parameter: " << e.what() << std::endl;
        return false;
    }
    return false;
}

std::string CameraParameter::getValue() const {
    try {
        switch (type) {
            case Type::Integer:
                return std::to_string(std::get<int>(value));
            case Type::Float:
                return std::to_string(std::get<double>(value));
            case Type::Boolean:
                return std::get<bool>(value) ? "true" : "false";
            case Type::String:
            case Type::Enumeration:
                return std::get<std::string>(value);
        }
    } catch (const std::exception& e) {
        std::cout << "Error getting parameter: " << e.what() << std::endl;
        return "";
    }
    return "";
}

// ImageBuffer implementation
ImageBuffer::ImageBuffer() 
    : width(0), height(0), pixelFormat(PixelFormat::Mono8), 
      timestamp(std::chrono::system_clock::now()) {
}

ImageBuffer::ImageBuffer(uint32_t w, uint32_t h, PixelFormat format)
    : width(w), height(h), pixelFormat(format),
      timestamp(std::chrono::system_clock::now()) {
    
    // Calculate buffer size (simplified)
    size_t bytesPerPixel = 1; // Default for Mono8
    switch (format) {
        case PixelFormat::Mono8:
        case PixelFormat::BayerRG8:
            bytesPerPixel = 1;
            break;
        case PixelFormat::Mono10:
        case PixelFormat::Mono12:
            bytesPerPixel = 2;
            break;
        case PixelFormat::RGB8:
            bytesPerPixel = 3;
            break;
    }
    
    data.resize(w * h * bytesPerPixel);
}

size_t ImageBuffer::getDataSize() const {
    return data.size();
}

// CaptureStatistics implementation
void CaptureStatistics::updateCapture(bool success, double captureTimeMs) {
    totalCaptured++;
    if (success) {
        successfulCaptures++;
    } else {
        failedCaptures++;
    }
    
    lastCaptureTime = std::chrono::system_clock::now();
    
    // Simple FPS calculation
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFPSUpdate);
    
    if (duration.count() >= 1000) { // Update every second
        averageFPS = static_cast<double>(recentCaptures) / (duration.count() / 1000.0);
        recentCaptures = 0;
        lastFPSUpdate = now;
    } else {
        recentCaptures++;
    }
}

void CaptureStatistics::reset() {
    totalCaptured = 0;
    successfulCaptures = 0;
    failedCaptures = 0;
    averageFPS = 0.0;
    lastCaptureTime = std::chrono::system_clock::now();
    lastFPSUpdate = std::chrono::high_resolution_clock::now();
    recentCaptures = 0;
}

double CaptureStatistics::getSuccessRate() const {
    if (totalCaptured == 0) return 0.0;
    return static_cast<double>(successfulCaptures) / totalCaptured * 100.0;
}

} // namespace SaperaCapturePro 