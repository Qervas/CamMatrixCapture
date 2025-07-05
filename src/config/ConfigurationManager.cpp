/**
 * Professional Configuration Manager Implementation
 * Handles system and camera configuration with validation and hot-reload
 */

#include "ConfigurationManager.hpp"
#include "../utils/JsonHelper.hpp"
#include "../utils/FileSystem.hpp"
#include "../utils/Logger.hpp"
#include <algorithm>

namespace SaperaCapturePro {

ConfigurationManager& ConfigurationManager::getInstance() {
    static ConfigurationManager instance;
    return instance;
}

ConfigurationManager::ConfigurationManager() 
    : configLoaded_(false), fileWatcher_(nullptr) {
    // Initialize with default values
    systemConfig_.application.name = "SaperaCapturePro";
    systemConfig_.application.version = "1.0.0";
    systemConfig_.server.port = 8080;
    systemConfig_.server.host = "0.0.0.0";
    systemConfig_.server.staticPath = "resources/web";
    systemConfig_.logging.level = "INFO";
    systemConfig_.logging.filePath = "logs/capture.log";
    systemConfig_.logging.maxFileSize = 10485760; // 10MB
    systemConfig_.logging.maxFiles = 5;
}

ConfigurationManager::~ConfigurationManager() {
    if (fileWatcher_) {
        fileWatcher_->stop();
    }
}

bool ConfigurationManager::loadConfiguration(const std::string& configPath) {
    configFilePath_ = configPath;
    
    if (!FileSystem::exists(configPath)) {
        Logger::error("Configuration file not found: " + configPath);
        return createDefaultConfiguration();
    }
    
    Logger::info("Loading configuration from: " + configPath);
    
    JsonValue configJson = JsonHelper::parseFile(configPath);
    if (configJson.isNull()) {
        Logger::error("Failed to parse configuration file: " + JsonHelper::getLastError());
        return false;
    }
    
    if (!configJson.isObject()) {
        Logger::error("Configuration file must contain a JSON object");
        return false;
    }
    
    return parseConfiguration(configJson.getObject());
}

bool ConfigurationManager::saveConfiguration(const std::string& configPath) {
    std::string path = configPath.empty() ? configFilePath_ : configPath;
    
    if (path.empty()) {
        Logger::error("No configuration file path specified");
        return false;
    }
    
    // Create directory if it doesn't exist
    std::string configDir = FileSystem::getParentDirectory(path);
    if (!configDir.empty() && !FileSystem::exists(configDir)) {
        FileSystem::createDirectories(configDir);
    }
    
    JsonObject configJson = createConfigurationJson();
    JsonValue configValue(configJson);
    
    if (JsonHelper::saveToFile(configValue, path, true)) {
        Logger::info("Configuration saved to: " + path);
        configFilePath_ = path;
        return true;
    } else {
        Logger::error("Failed to save configuration: " + JsonHelper::getLastError());
        return false;
    }
}

bool ConfigurationManager::reloadConfiguration() {
    if (configFilePath_.empty()) {
        Logger::error("No configuration file path available for reload");
        return false;
    }
    
    Logger::info("Reloading configuration from: " + configFilePath_);
    return loadConfiguration(configFilePath_);
}

// Configuration access methods
const SystemConfiguration& ConfigurationManager::getSystemConfig() const {
    return systemConfig_;
}

const std::vector<CameraConfiguration>& ConfigurationManager::getCameraConfigs() const {
    return cameraConfigs_;
}

bool ConfigurationManager::getCameraConfig(const std::string& cameraId, CameraConfiguration& config) const {
    auto it = std::find_if(cameraConfigs_.begin(), cameraConfigs_.end(),
                          [&cameraId](const CameraConfiguration& cam) {
                              return cam.id == cameraId;
                          });
    
    if (it != cameraConfigs_.end()) {
        config = *it;
        return true;
    }
    
    return false;
}

bool ConfigurationManager::updateCameraConfig(const CameraConfiguration& config) {
    auto it = std::find_if(cameraConfigs_.begin(), cameraConfigs_.end(),
                          [&config](const CameraConfiguration& cam) {
                              return cam.id == config.id;
                          });
    
    if (it != cameraConfigs_.end()) {
        *it = config;
        Logger::info("Updated configuration for camera: " + config.id);
        return true;
    } else {
        cameraConfigs_.push_back(config);
        Logger::info("Added new camera configuration: " + config.id);
        return true;
    }
}

void ConfigurationManager::updateSystemConfig(const SystemConfiguration& config) {
    systemConfig_ = config;
    Logger::info("System configuration updated");
}

// Validation
bool ConfigurationManager::validateConfiguration() const {
    // Validate system configuration
    if (systemConfig_.application.name.empty()) {
        Logger::error("Application name cannot be empty");
        return false;
    }
    
    if (systemConfig_.server.port < 1 || systemConfig_.server.port > 65535) {
        Logger::error("Invalid server port: " + std::to_string(systemConfig_.server.port));
        return false;
    }
    
    Logger::info("Configuration validation passed");
    return true;
}

// Private methods
bool ConfigurationManager::parseConfiguration(const JsonObject& configJson) {
    try {
        // Parse application section
        if (configJson.has("application")) {
            JsonObject appJson = configJson.get("application").getObject();
            systemConfig_.application.name = appJson.get("name", JsonValue(systemConfig_.application.name)).getString();
            systemConfig_.application.version = appJson.get("version", JsonValue(systemConfig_.application.version)).getString();
            systemConfig_.application.description = appJson.get("description", JsonValue("")).getString();
        }
        
        // Parse server section
        if (configJson.has("server")) {
            JsonObject serverJson = configJson.get("server").getObject();
            systemConfig_.server.port = serverJson.get("port", JsonValue(systemConfig_.server.port)).getInt();
            systemConfig_.server.host = serverJson.get("host", JsonValue(systemConfig_.server.host)).getString();
            systemConfig_.server.staticPath = serverJson.get("staticPath", JsonValue(systemConfig_.server.staticPath)).getString();
            systemConfig_.server.enableCors = serverJson.get("enableCors", JsonValue(true)).getBool();
            systemConfig_.server.maxConnections = serverJson.get("maxConnections", JsonValue(100)).getInt();
        }
        
        // Parse logging section
        if (configJson.has("logging")) {
            JsonObject logJson = configJson.get("logging").getObject();
            systemConfig_.logging.level = logJson.get("level", JsonValue(systemConfig_.logging.level)).getString();
            systemConfig_.logging.filePath = logJson.get("filePath", JsonValue(systemConfig_.logging.filePath)).getString();
            systemConfig_.logging.enableConsole = logJson.get("enableConsole", JsonValue(true)).getBool();
            systemConfig_.logging.maxFileSize = logJson.get("maxFileSize", JsonValue(systemConfig_.logging.maxFileSize)).getInt();
            systemConfig_.logging.maxFiles = logJson.get("maxFiles", JsonValue(systemConfig_.logging.maxFiles)).getInt();
        }
        
        configLoaded_ = true;
        Logger::info("Configuration loaded successfully");
        return validateConfiguration();
        
    } catch (const std::exception& e) {
        Logger::error("Error parsing configuration: " + std::string(e.what()));
        return false;
    }
}

JsonObject ConfigurationManager::createConfigurationJson() const {
    JsonObject configJson;
    
    // Application section
    JsonObject appJson;
    appJson.set("name", JsonValue(systemConfig_.application.name));
    appJson.set("version", JsonValue(systemConfig_.application.version));
    appJson.set("description", JsonValue(systemConfig_.application.description));
    configJson.set("application", JsonValue(appJson));
    
    // Server section
    JsonObject serverJson;
    serverJson.set("port", JsonValue(systemConfig_.server.port));
    serverJson.set("host", JsonValue(systemConfig_.server.host));
    serverJson.set("staticPath", JsonValue(systemConfig_.server.staticPath));
    serverJson.set("enableCors", JsonValue(systemConfig_.server.enableCors));
    serverJson.set("maxConnections", JsonValue(systemConfig_.server.maxConnections));
    configJson.set("server", JsonValue(serverJson));
    
    // Logging section
    JsonObject logJson;
    logJson.set("level", JsonValue(systemConfig_.logging.level));
    logJson.set("filePath", JsonValue(systemConfig_.logging.filePath));
    logJson.set("enableConsole", JsonValue(systemConfig_.logging.enableConsole));
    logJson.set("maxFileSize", JsonValue(systemConfig_.logging.maxFileSize));
    logJson.set("maxFiles", JsonValue(systemConfig_.logging.maxFiles));
    configJson.set("logging", JsonValue(logJson));
    
    return configJson;
}

bool ConfigurationManager::createDefaultConfiguration() {
    Logger::info("Creating default configuration");
    
    configLoaded_ = true;
    
    // Save the default configuration
    return !configFilePath_.empty() ? saveConfiguration() : true;
}

} // namespace SaperaCapturePro 