#include "SettingsManager.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <iostream>

// CameraSettings implementation
SimpleJSON CameraSettings::ToJson() const {
    SimpleJSON json;
    
    // Image Format & Resolution
    json.set("width", width);
    json.set("height", height);
    json.set("pixel_format", pixel_format);
    
    // Acquisition Control
    json.set("exposure_time", exposure_time);
    json.set("gain", gain);
    json.set("auto_exposure", auto_exposure);
    json.set("auto_gain", auto_gain);
    json.set("acquisition_preset", acquisition_preset);
    
    // Trigger Control
    json.set("trigger_mode", trigger_mode);
    json.set("trigger_source", trigger_source);
    json.set("trigger_activation", trigger_activation);
    json.set("trigger_delay", trigger_delay);
    
    // Color Control
    json.set("white_balance_red", white_balance_red);
    json.set("white_balance_green", white_balance_green);
    json.set("white_balance_blue", white_balance_blue);
    json.set("auto_white_balance", auto_white_balance);
    json.set("saturation", saturation);
    json.set("hue", hue);
    json.set("gamma", gamma);
    
    // Advanced Features
    json.set("acquisition_mode", acquisition_mode);
    json.set("acquisition_frame_count", acquisition_frame_count);
    // Frame rate settings removed
    
    // Region of Interest (ROI)
    json.set("roi_offset_x", roi_offset_x);
    json.set("roi_offset_y", roi_offset_y);
    json.set("roi_width", roi_width);
    json.set("roi_height", roi_height);
    json.set("roi_enabled", roi_enabled);
    
    // Transport Layer
    json.set("packet_size", packet_size);
    json.set("packet_delay", packet_delay);
    
    return json;
}

void CameraSettings::FromJson(const SimpleJSON& json) {
    // Image Format & Resolution
    width = json.getInt("width", width);
    height = json.getInt("height", height);
    pixel_format = json.get("pixel_format", pixel_format);
    
    // Acquisition Control
    exposure_time = json.getInt("exposure_time", exposure_time);
    gain = json.getFloat("gain", gain);
    auto_exposure = json.getBool("auto_exposure", auto_exposure);
    auto_gain = json.getBool("auto_gain", auto_gain);
    acquisition_preset = json.get("acquisition_preset", acquisition_preset);
    
    // Trigger Control
    trigger_mode = json.get("trigger_mode", trigger_mode);
    trigger_source = json.get("trigger_source", trigger_source);
    trigger_activation = json.get("trigger_activation", trigger_activation);
    trigger_delay = json.getFloat("trigger_delay", trigger_delay);
    
    // Color Control
    white_balance_red = json.getFloat("white_balance_red", white_balance_red);
    white_balance_green = json.getFloat("white_balance_green", white_balance_green);
    white_balance_blue = json.getFloat("white_balance_blue", white_balance_blue);
    auto_white_balance = json.getBool("auto_white_balance", auto_white_balance);
    saturation = json.getFloat("saturation", saturation);
    hue = json.getFloat("hue", hue);
    gamma = json.getFloat("gamma", gamma);
    
    // Advanced Features
    acquisition_mode = json.get("acquisition_mode", acquisition_mode);
    acquisition_frame_count = json.getInt("acquisition_frame_count", acquisition_frame_count);
    // Frame rate settings removed
    
    // Region of Interest (ROI)
    roi_offset_x = json.getInt("roi_offset_x", roi_offset_x);
    roi_offset_y = json.getInt("roi_offset_y", roi_offset_y);
    roi_width = json.getInt("roi_width", roi_width);
    roi_height = json.getInt("roi_height", roi_height);
    roi_enabled = json.getBool("roi_enabled", roi_enabled);
    
    // Transport Layer
    packet_size = json.getInt("packet_size", packet_size);
    packet_delay = json.getInt("packet_delay", packet_delay);
}

void CameraSettings::Reset() {
    *this = CameraSettings{}; // Reset to defaults
}

// IndividualCameraSettings implementation - NEW
SimpleJSON IndividualCameraSettings::ToJson() const {
    SimpleJSON json;
    
    // Camera ID
    json.set("camera_id", camera_id);
    
    // Acquisition Control
    json.set("exposure_time", exposure_time);
    json.set("gain", gain);
    json.set("auto_exposure", auto_exposure);
    json.set("auto_gain", auto_gain);
    
    // Color Control
    json.set("white_balance_red", white_balance_red);
    json.set("white_balance_green", white_balance_green);
    json.set("white_balance_blue", white_balance_blue);
    json.set("auto_white_balance", auto_white_balance);
    json.set("saturation", saturation);
    json.set("hue", hue);
    json.set("gamma", gamma);
    
    // Advanced Features
    json.set("acquisition_mode", acquisition_mode);
    json.set("acquisition_frame_count", acquisition_frame_count);
    // Frame rate settings removed
    
    // Region of Interest (ROI)
    json.set("roi_offset_x", roi_offset_x);
    json.set("roi_offset_y", roi_offset_y);
    json.set("roi_width", roi_width);
    json.set("roi_height", roi_height);
    json.set("roi_enabled", roi_enabled);
    
    return json;
}

void IndividualCameraSettings::FromJson(const SimpleJSON& json) {
    // Camera ID
    camera_id = json.get("camera_id", camera_id);
    
    // Acquisition Control
    exposure_time = json.getInt("exposure_time", exposure_time);
    gain = json.getFloat("gain", gain);
    auto_exposure = json.getBool("auto_exposure", auto_exposure);
    auto_gain = json.getBool("auto_gain", auto_gain);
    
    // Color Control
    white_balance_red = json.getFloat("white_balance_red", white_balance_red);
    white_balance_green = json.getFloat("white_balance_green", white_balance_green);
    white_balance_blue = json.getFloat("white_balance_blue", white_balance_blue);
    auto_white_balance = json.getBool("auto_white_balance", auto_white_balance);
    saturation = json.getFloat("saturation", saturation);
    hue = json.getFloat("hue", hue);
    gamma = json.getFloat("gamma", gamma);
    
    // Advanced Features
    acquisition_mode = json.get("acquisition_mode", acquisition_mode);
    acquisition_frame_count = json.getInt("acquisition_frame_count", acquisition_frame_count);
    // Frame rate settings removed
    
    // Region of Interest (ROI)
    roi_offset_x = json.getInt("roi_offset_x", roi_offset_x);
    roi_offset_y = json.getInt("roi_offset_y", roi_offset_y);
    roi_width = json.getInt("roi_width", roi_width);
    roi_height = json.getInt("roi_height", roi_height);
    roi_enabled = json.getBool("roi_enabled", roi_enabled);
}

void IndividualCameraSettings::Reset() {
    *this = IndividualCameraSettings{}; // Reset to defaults
}

// AppSettings implementation
SimpleJSON AppSettings::ToJson() const {
    SimpleJSON json;
    json.set("last_output_folder", last_output_folder);
    json.set("auto_save_settings", auto_save_settings);
    json.set("dark_theme", dark_theme);
    json.set("window_width", window_width);
    json.set("window_height", window_height);
    json.set("window_x", window_x);
    json.set("window_y", window_y);
    json.set("ui_scale", ui_scale);
    json.set("vsync", vsync);
    json.set("last_bluetooth_device_id", last_bluetooth_device_id);
    json.set("last_bluetooth_device_name", last_bluetooth_device_name);
    json.set("auto_connect_enabled", auto_connect_enabled);
    return json;
}

void AppSettings::FromJson(const SimpleJSON& json) {
    last_output_folder = json.get("last_output_folder", last_output_folder);
    auto_save_settings = json.getBool("auto_save_settings", auto_save_settings);
    dark_theme = json.getBool("dark_theme", dark_theme);
    window_width = json.getInt("window_width", window_width);
    window_height = json.getInt("window_height", window_height);
    window_x = json.getInt("window_x", window_x);
    window_y = json.getInt("window_y", window_y);
    vsync = json.getBool("vsync", vsync);
    
    // Load connection info
    last_bluetooth_device_id = json.get("last_bluetooth_device_id", last_bluetooth_device_id);
    last_bluetooth_device_name = json.get("last_bluetooth_device_name", last_bluetooth_device_name);
    auto_connect_enabled = json.getBool("auto_connect_enabled", auto_connect_enabled);
    
    // Load and validate UI scale
    float loaded_scale = json.getFloat("ui_scale", ui_scale);
    // Clamp to valid range
    ui_scale = std::min(std::max(loaded_scale, 0.5f), 8.0f);
}

void AppSettings::Reset() {
    *this = AppSettings{}; // Reset to defaults
}

// SettingsManager implementation
SettingsManager::SettingsManager(const std::string& config_path)
    : config_file_path(config_path), auto_save_enabled(true) {
    
    // Create config directory if it doesn't exist
    try {
        std::filesystem::create_directories(std::filesystem::path(config_path).parent_path());
    } catch (const std::exception&) {
        // Directory creation failed, but continue
    }
    
    LoadFromFile();
}

SettingsManager::~SettingsManager() {
    if (auto_save_enabled) {
        SaveToFile();
    }
}

void SettingsManager::ResetCameraSettings() {
    camera_settings.Reset();
}

// Individual Camera Settings Implementation - NEW
IndividualCameraSettings& SettingsManager::GetIndividualCameraSettings(const std::string& camera_id) {
    if (individual_camera_settings.find(camera_id) == individual_camera_settings.end()) {
        // Create default settings for this camera
        IndividualCameraSettings defaults;
        defaults.camera_id = camera_id;
        individual_camera_settings[camera_id] = defaults;
    }
    return individual_camera_settings[camera_id];
}

const IndividualCameraSettings& SettingsManager::GetIndividualCameraSettings(const std::string& camera_id) const {
    auto it = individual_camera_settings.find(camera_id);
    if (it != individual_camera_settings.end()) {
        return it->second;
    }
    
    // Return a default static instance if not found
    static IndividualCameraSettings default_settings;
    return default_settings;
}

void SettingsManager::SetIndividualCameraSettings(const std::string& camera_id, const IndividualCameraSettings& settings) {
    individual_camera_settings[camera_id] = settings;
    individual_camera_settings[camera_id].camera_id = camera_id; // Ensure camera_id is set correctly
}

void SettingsManager::RemoveIndividualCameraSettings(const std::string& camera_id) {
    individual_camera_settings.erase(camera_id);
}

std::vector<std::string> SettingsManager::GetIndividualCameraIds() const {
    std::vector<std::string> ids;
    for (const auto& [id, settings] : individual_camera_settings) {
        ids.push_back(id);
    }
    return ids;
}

void SettingsManager::ResetIndividualCameraSettings(const std::string& camera_id) {
    auto it = individual_camera_settings.find(camera_id);
    if (it != individual_camera_settings.end()) {
        it->second.Reset();
        it->second.camera_id = camera_id; // Restore camera ID after reset
    }
}

void SettingsManager::ResetAllIndividualCameraSettings() {
    for (auto& [id, settings] : individual_camera_settings) {
        settings.Reset();
        settings.camera_id = id; // Restore camera ID after reset
    }
}

bool SettingsManager::HasIndividualCameraSettings(const std::string& camera_id) const {
    return individual_camera_settings.find(camera_id) != individual_camera_settings.end();
}

void SettingsManager::ResetAppSettings() {
    app_settings.Reset();
}

bool SettingsManager::Save() {
    std::cout << "[SETTINGS] Save() method called" << std::endl;
    return SaveToFile();
}

bool SettingsManager::Load() {
    return LoadFromFile();
}

void SettingsManager::ResetAllSettings() {
    ResetCameraSettings();
    ResetAppSettings();
}

void SettingsManager::SetConfigPath(const std::string& path) {
    config_file_path = path;
}

bool SettingsManager::LoadFromFile() {
    try {
        std::ifstream file(config_file_path);
        if (!file.is_open()) {
            return false; // File doesn't exist yet, use defaults
        }
        
        SimpleJSON json;
        std::string line;
        
        while (std::getline(file, line)) {
            auto pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                json.data[key] = value;
            }
        }
        
        // Load settings from JSON using the full settings parser
        LoadFullSettings(json);
        
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool SettingsManager::SaveToFile() const {
    std::cout << "[SETTINGS] SaveToFile called - writing to: " << config_file_path << std::endl;
    
    try {
        std::ofstream file(config_file_path);
        if (!file.is_open()) {
            std::cout << "[SETTINGS] ERROR: Could not open file for writing" << std::endl;
            return false;
        }
        
        // Combine all settings into one JSON
        auto full_json = CreateFullSettings();
        
        std::cout << "[SETTINGS] Writing " << full_json.data.size() << " settings to file" << std::endl;
        
        for (const auto& [key, value] : full_json.data) {
            file << key << "=" << value << std::endl;
        }
        
        std::cout << "[SETTINGS] Settings saved successfully" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cout << "[SETTINGS] ERROR saving: " << e.what() << std::endl;
        return false;
    }
}

SimpleJSON SettingsManager::CreateFullSettings() const {
    SimpleJSON combined;
    
    // Merge camera settings
    auto camera_json = camera_settings.ToJson();
    for (const auto& [key, value] : camera_json.data) {
        combined.data["camera_" + key] = value;
    }
    
    // Merge app settings
    auto app_json = app_settings.ToJson();
    for (const auto& [key, value] : app_json.data) {
        combined.data["app_" + key] = value;
    }
    
    // Merge individual camera settings - NEW
    for (const auto& [camera_id, settings] : individual_camera_settings) {
        auto individual_json = settings.ToJson();
        for (const auto& [key, value] : individual_json.data) {
            combined.data["individual_" + camera_id + "_" + key] = value;
        }
    }
    
    return combined;
}

void SettingsManager::LoadFullSettings(const SimpleJSON& json) {
    SimpleJSON camera_json, app_json;
    std::map<std::string, SimpleJSON> individual_jsons;
    
    for (const auto& [key, value] : json.data) {
        if (key.starts_with("camera_")) {
            camera_json.data[key.substr(7)] = value; // Remove "camera_" prefix
        } else if (key.starts_with("app_")) {
            app_json.data[key.substr(4)] = value; // Remove "app_" prefix
        } else if (key.starts_with("individual_")) {
            // Parse individual camera settings: "individual_CAMERAID_PARAMETER"
            std::string remaining = key.substr(11); // Remove "individual_" prefix
            auto second_underscore = remaining.find('_');
            if (second_underscore != std::string::npos) {
                std::string camera_id = remaining.substr(0, second_underscore);
                std::string param_name = remaining.substr(second_underscore + 1);
                individual_jsons[camera_id].data[param_name] = value;
            }
        }
    }
    
    camera_settings.FromJson(camera_json);
    app_settings.FromJson(app_json);
    
    // Load individual camera settings - NEW
    individual_camera_settings.clear();
    for (const auto& [camera_id, json_data] : individual_jsons) {
        IndividualCameraSettings settings;
        settings.FromJson(json_data);
        settings.camera_id = camera_id; // Ensure camera ID is set
        individual_camera_settings[camera_id] = settings;
    }
} 