#pragma once

#include <string>
#include <map>
#include <variant>
#include <fstream>
#include <filesystem>
#include "SessionManager.hpp"

// Settings value type that can hold different types
using SettingsValue = std::variant<int, float, bool, std::string>;

// Camera Ordering Entry - Maps serial number to display position
struct CameraOrderEntry {
    std::string serial_number;  // Hardware serial number (e.g., "S1128470")
    int display_position = 0;   // Display order (0-based, 0 = first camera)

    SimpleJSON ToJson() const;
    void FromJson(const SimpleJSON& json);
};

// Camera Ordering Settings - Maintains user-defined camera order
struct CameraOrderSettings {
    std::vector<CameraOrderEntry> order_entries;  // Ordered list of cameras
    bool use_custom_ordering = false;             // Enable/disable custom ordering

    SimpleJSON ToJson() const;
    void FromJson(const SimpleJSON& json);
    void Reset();

    // Helper methods
    int GetDisplayPosition(const std::string& serial_number) const;
    void SetDisplayPosition(const std::string& serial_number, int position);
    void RemoveCamera(const std::string& serial_number);
    bool HasCamera(const std::string& serial_number) const;
};

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
    // Frame rate controls removed for vanilla capture

    // Region of Interest (ROI)
    int roi_offset_x = 0;
    int roi_offset_y = 0;
    int roi_width = 1920;
    int roi_height = 1080;
    bool roi_enabled = false;

    // Crop settings (for preview and capture)
    bool crop_enabled = false;
    int crop_offset_x = 0;
    int crop_offset_y = 0;
    int crop_width = 4112;   // Full width by default
    int crop_height = 3008;  // Full height by default

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
    // Advanced Color Processing
    int color_method = 1;            // 1-7
    int bayer_align = 2;             // 0=GBRG,1=BGGR,2=RGGB,3=GRBG,4=RGBG,5=BGRG
    bool use_hardware_conversion = false;
    // Output format (SapFormat selection stored as string label)
    std::string color_output_format = "RGB888"; // RGB888, RGB8888, RGB101010
    // White balance offset per channel (for fine tuning)
    float white_balance_offset_red = 0.0f;
    float white_balance_offset_green = 0.0f;
    float white_balance_offset_blue = 0.0f;
    
    // Advanced Features
    std::string acquisition_mode = "Continuous";
    int acquisition_frame_count = 1;
    // Frame rate controls removed for vanilla capture
    
    // Region of Interest (ROI)
    int roi_offset_x = 0;
    int roi_offset_y = 0;
    int roi_width = 1920;
    int roi_height = 1080;
    bool roi_enabled = false;

    // Crop settings (for preview and capture)
    bool crop_enabled = false;
    int crop_offset_x = 0;
    int crop_offset_y = 0;
    int crop_width = 4112;   // Full width by default
    int crop_height = 3008;  // Full height by default
    bool crop_maintain_aspect = true;
    float crop_aspect_ratio = 1.37f; // 4112/3008 default ratio

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
    int window_x = 100;
    int window_y = 100;
    float ui_scale = 1.0f;  // UI scale factor (0.5 to 8.0)
    bool vsync = true;
    
    // Last connection info for Quick Connect
    std::string last_bluetooth_device_id = "";
    std::string last_bluetooth_device_name = "";
    bool auto_connect_enabled = true;

    // Log settings
    bool log_auto_delete_enabled = false;  // Default: no auto-deletion
    int log_max_messages = 0;  // 0 = unlimited

    // Notification sound settings
    bool enable_completion_sound = true;  // Enable sound when sequence completes
    std::string completion_sound = "Windows Ding";  // Sound name/type
    float notification_volume = 0.5f;  // Volume 0.0 to 1.0

    SimpleJSON ToJson() const;
    void FromJson(const SimpleJSON& json);
    void Reset();
};

class SettingsManager {
private:
    std::string config_file_path;
    CameraSettings camera_settings;
    AppSettings app_settings;
    CameraOrderSettings camera_order_settings; // NEW: Camera ordering
    std::map<std::string, IndividualCameraSettings> individual_camera_settings; // NEW: Per-camera settings
    bool auto_save_enabled;

    bool LoadFromFile();
    bool SaveToFile() const;
    SimpleJSON CreateFullSettings() const;
    void LoadFullSettings(const SimpleJSON& json);

public:
    explicit SettingsManager(const std::string& config_path = "settings.ini");
    ~SettingsManager();

    // Camera Settings
    CameraSettings& GetCameraSettings() { return camera_settings; }
    const CameraSettings& GetCameraSettings() const { return camera_settings; }
    void ResetCameraSettings();

    // Camera Ordering Settings - NEW
    CameraOrderSettings& GetCameraOrderSettings() { return camera_order_settings; }
    const CameraOrderSettings& GetCameraOrderSettings() const { return camera_order_settings; }
    void ResetCameraOrderSettings();
    bool LoadCameraConfigJson(const std::string& config_path = "config/camera_config.json");

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