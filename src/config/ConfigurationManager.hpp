/**
 * Professional Configuration Management System
 * Handles system-wide configuration with validation and type safety
 */

#pragma once

#include "../utils/JsonHelper.hpp"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <set>
#include "../utils/FileSystem.hpp"

namespace SaperaCapturePro {

/**
 * Camera configuration structure
 */
struct CameraConfiguration {
    std::string id;                   // Camera ID
    std::string name;                 // Display name
    std::map<std::string, std::string> parameters; // Key-value parameter pairs
    
    // Default constructor
    CameraConfiguration() = default;
    
    // Constructor with ID
    CameraConfiguration(const std::string& cameraId) : id(cameraId) {}
};

/**
 * System-wide configuration
 */
struct SystemConfiguration {
    struct ApplicationConfig {
        std::string name = "SaperaCapturePro";
        std::string version = "1.0.0";
        std::string description = "Professional Camera Capture System";
    } application;
    
    struct ServerConfig {
        int port = 8080;
        std::string host = "0.0.0.0";
        std::string staticPath = "resources/web";
        bool enableCors = true;
        int maxConnections = 100;
    } server;
    
    struct LoggingConfig {
        std::string level = "INFO";
        std::string filePath = "logs/capture.log";
        bool enableConsole = true;
        int maxFileSize = 10485760; // 10MB
        int maxFiles = 5;
    } logging;
};

/**
 * Main configuration manager (Singleton)
 */
class ConfigurationManager {
public:
    static ConfigurationManager& getInstance();
    
    // Configuration loading and saving
    bool loadConfiguration(const std::string& filePath);
    bool saveConfiguration(const std::string& filePath = "") const;
    bool reloadConfiguration();
    
    // System configuration access
    const SystemConfiguration& getSystemConfig() const { return systemConfig_; }
    void updateSystemConfig(const SystemConfiguration& config);
    
    // Camera configuration access
    const std::vector<CameraConfiguration>& getCameraConfigs() const { return cameraConfigs_; }
    bool getCameraConfig(const std::string& cameraId, CameraConfiguration& config) const;
    bool updateCameraConfig(const CameraConfiguration& config);
    void updateSystemConfig(const SystemConfiguration& config);
    
    // Preset management
    bool savePreset(const std::string& name, const std::map<std::string, JsonValue>& parameters);
    bool loadPreset(const std::string& name, std::map<std::string, JsonValue>& parameters) const;
    std::vector<std::string> getPresetNames() const;
    bool deletePreset(const std::string& name);
    
    // Configuration validation
    bool validateConfiguration() const;
    std::vector<std::string> getValidationErrors() const;
    
    // Change notification
    using ConfigChangeCallback = std::function<void(const std::string& section)>;
    void registerChangeCallback(const ConfigChangeCallback& callback);
    
    // Default configuration generation
    bool generateDefaultConfiguration(const std::string& filePath);
    
private:
    ConfigurationManager() = default;
    ~ConfigurationManager() = default;
    ConfigurationManager(const ConfigurationManager&) = delete;
    ConfigurationManager& operator=(const ConfigurationManager&) = delete;
    
    void notifyConfigChange(const std::string& section);
    bool validateCameraConfigs() const;
    bool validateSystemConfig() const;
    
    // Private methods
    bool parseConfiguration(const JsonObject& configJson);
    JsonObject createConfigurationJson() const;
    bool createDefaultConfiguration();

    // Member variables
    SystemConfiguration systemConfig_;
    std::vector<CameraConfiguration> cameraConfigs_;
    std::map<std::string, std::vector<CameraConfiguration>> presets_;
    
    bool configLoaded_;
    std::string configFilePath_;
    std::unique_ptr<FileSystem::FileWatcher> fileWatcher_;
    
    mutable std::mutex configMutex_;
    
    std::vector<ConfigChangeCallback> changeCallbacks_;
    mutable std::vector<std::string> lastValidationErrors_;
};

} // namespace SaperaCapturePro 