#pragma once

#include <string>
#include <map>
#include <variant>
#include <fstream>
#include <filesystem>
#include "SessionManager.hpp"

// Settings value type that can hold different types
using SettingsValue = std::variant<int, float, bool, std::string>;

// Individual Camera Settings for per-camera parameter control - NEW
struct IndividualCameraSettings {
    std::string camera_id;
    
    // Acquisition Control
    int exposure_time = 40000;  // 40k microseconds default
    float gain = 1.0f;
    bool auto_exposure = false;
    bool auto_gain = false;
    
    // Color Control
    float white_balance_red = 1.0f;
    float white_balance_green = 1.0f;
    float white_balance_blue = 1.0f;
    bool auto_white_balance = false;
    float saturation = 1.0f;
    float hue = 0.0f;
    float gamma = 1.0f;
    
    // Advanced Features
    std::string acquisition_mode = "Continuous";
    int acquisition_frame_count = 1;
    float acquisition_frame_rate = 30.0f;
    bool frame_rate_enable = false;
    
    // Region of Interest (ROI)
    int roi_offset_x = 0;
    int roi_offset_y = 0;
    int roi_width = 1920;
    int roi_height = 1080;
    bool roi_enabled = false;
    
    SimpleJSON ToJson() const;
    void FromJson(const SimpleJSON& json);
    void Reset();
};

// Camera parameter structure for persistence
struct CameraSettings {
    // Image Format & Resolution
    int width = 4112;
    int height = 3008;
    std::string pixel_format = "RGB8";
    
    // Acquisition Control
    int exposure_time = 40000;  // 40k microseconds default
    float gain = 1.0f;
    bool auto_exposure = false;
    bool auto_gain = false;
    std::string acquisition_preset = "Default";
    
    // Trigger Control
    std::string trigger_mode = "Off";
    std::string trigger_source = "Software";
    std::string trigger_activation = "RisingEdge";
    float trigger_delay = 0.0f;
    
    // Color Control
    float white_balance_red = 1.0f;
    float white_balance_green = 1.0f;
    float white_balance_blue = 1.0f;
    bool auto_white_balance = false;
    float saturation = 1.0f;
    float hue = 0.0f;
    float gamma = 1.0f;
    
    // Advanced Features
    std::string acquisition_mode = "Continuous";
    int acquisition_frame_count = 1;
    float acquisition_frame_rate = 30.0f;
    bool frame_rate_enable = false;
    
    // Region of Interest (ROI)
    int roi_offset_x = 0;
    int roi_offset_y = 0;
    int roi_width = 1920;
    int roi_height = 1080;
    bool roi_enabled = false;
    
    // Transport Layer
    int packet_size = 1500;
    int packet_delay = 0;
    
    SimpleJSON ToJson() const;
    void FromJson(const SimpleJSON& json);
    void Reset();
};

// General application settings
struct AppSettings {
    std::string last_output_folder = "neural_dataset";
    bool auto_save_settings = true;
    bool dark_theme = true;
    int window_width = 1200;
    int window_height = 800;
    
    SimpleJSON ToJson() const;
    void FromJson(const SimpleJSON& json);
    void Reset();
};

class SettingsManager {
private:
    std::string config_file_path;
    CameraSettings camera_settings;
    AppSettings app_settings;
    std::map<std::string, IndividualCameraSettings> individual_camera_settings; // NEW: Per-camera settings
    bool auto_save_enabled;
    
    bool LoadFromFile();
    bool SaveToFile() const;
    SimpleJSON CreateFullSettings() const;
    void LoadFullSettings(const SimpleJSON& json);
    
public:
    explicit SettingsManager(const std::string& config_path = "config/settings.json");
    ~SettingsManager();
    
    // Camera Settings
    CameraSettings& GetCameraSettings() { return camera_settings; }
    const CameraSettings& GetCameraSettings() const { return camera_settings; }
    void ResetCameraSettings();
    
    // Individual Camera Settings - NEW
    IndividualCameraSettings& GetIndividualCameraSettings(const std::string& camera_id);
    const IndividualCameraSettings& GetIndividualCameraSettings(const std::string& camera_id) const;
    void SetIndividualCameraSettings(const std::string& camera_id, const IndividualCameraSettings& settings);
    void RemoveIndividualCameraSettings(const std::string& camera_id);
    std::vector<std::string> GetIndividualCameraIds() const;
    void ResetIndividualCameraSettings(const std::string& camera_id);
    void ResetAllIndividualCameraSettings();
    bool HasIndividualCameraSettings(const std::string& camera_id) const;
    
    // App Settings
    AppSettings& GetAppSettings() { return app_settings; }
    const AppSettings& GetAppSettings() const { return app_settings; }
    void ResetAppSettings();
    
    // General Operations
    bool Save();
    bool Load();
    void ResetAllSettings();
    
    // Auto-save
    void SetAutoSave(bool enabled) { auto_save_enabled = enabled; }
    bool IsAutoSaveEnabled() const { return auto_save_enabled; }
    
    // Utility
    std::string GetConfigPath() const { return config_file_path; }
    void SetConfigPath(const std::string& path);
}; 