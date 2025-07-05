/**
 * Configuration.cpp - Modern Configuration System Implementation
 */

#include "Configuration.hpp"
#include <fstream>
#include <format>
#include <iostream>

namespace sapera::utils {

// =============================================================================
// CAMERA SYSTEM CONFIG IMPLEMENTATION
// =============================================================================

core::VoidResult CameraSystemConfig::validate() const {
    if (discovery_timeout && discovery_timeout->count() < 1000) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
            "Discovery timeout must be at least 1000ms"));
    }
    
    if (connection_timeout && connection_timeout->count() < 1000) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
            "Connection timeout must be at least 1000ms"));
    }
    
    if (max_connection_retries && *max_connection_retries > 10) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
            "Max connection retries cannot exceed 10"));
    }
    
    if (default_buffer_count && *default_buffer_count < 1) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
            "Default buffer count must be at least 1"));
    }
    
    if (max_buffer_count && *max_buffer_count > 20) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
            "Max buffer count cannot exceed 20"));
    }
    
    if (max_concurrent_captures && *max_concurrent_captures > 10) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
            "Max concurrent captures cannot exceed 10"));
    }
    
    return core::Ok();
}

core::Result<CameraSystemConfig> CameraSystemConfig::from_json(const nlohmann::json& j) {
    CameraSystemConfig config;
    
    try {
        if (j.contains("discovery_timeout")) {
            config.discovery_timeout = std::chrono::milliseconds(j["discovery_timeout"].get<int>());
        }
        if (j.contains("auto_refresh_camera_list")) {
            config.auto_refresh_camera_list = j["auto_refresh_camera_list"].get<bool>();
        }
        if (j.contains("refresh_interval")) {
            config.refresh_interval = std::chrono::seconds(j["refresh_interval"].get<int>());
        }
        if (j.contains("connection_timeout")) {
            config.connection_timeout = std::chrono::milliseconds(j["connection_timeout"].get<int>());
        }
        if (j.contains("max_connection_retries")) {
            config.max_connection_retries = j["max_connection_retries"].get<uint32_t>();
        }
        if (j.contains("retry_delay")) {
            config.retry_delay = std::chrono::milliseconds(j["retry_delay"].get<int>());
        }
        if (j.contains("enable_auto_reconnect")) {
            config.enable_auto_reconnect = j["enable_auto_reconnect"].get<bool>();
        }
        if (j.contains("default_buffer_count")) {
            config.default_buffer_count = j["default_buffer_count"].get<uint32_t>();
        }
        if (j.contains("max_buffer_count")) {
            config.max_buffer_count = j["max_buffer_count"].get<uint32_t>();
        }
        if (j.contains("capture_timeout")) {
            config.capture_timeout = std::chrono::milliseconds(j["capture_timeout"].get<int>());
        }
        if (j.contains("enable_performance_monitoring")) {
            config.enable_performance_monitoring = j["enable_performance_monitoring"].get<bool>();
        }
        if (j.contains("performance_log_interval")) {
            config.performance_log_interval = std::chrono::seconds(j["performance_log_interval"].get<int>());
        }
        if (j.contains("max_concurrent_captures")) {
            config.max_concurrent_captures = j["max_concurrent_captures"].get<uint32_t>();
        }
        
        if (auto result = config.validate(); !result.has_value()) {
            return core::Err<CameraSystemConfig>(result.error());
        }
        
        return core::Ok(config);
    } catch (const nlohmann::json::exception& e) {
        return core::Err<CameraSystemConfig>(core::make_error(core::ErrorCode::InvalidConfiguration,
            "JSON parsing error in CameraSystemConfig: " + std::string(e.what())));
    }
}

nlohmann::json CameraSystemConfig::to_json() const {
    nlohmann::json j;
    
    if (discovery_timeout) j["discovery_timeout"] = discovery_timeout->count();
    if (auto_refresh_camera_list) j["auto_refresh_camera_list"] = *auto_refresh_camera_list;
    if (refresh_interval) j["refresh_interval"] = refresh_interval->count();
    if (connection_timeout) j["connection_timeout"] = connection_timeout->count();
    if (max_connection_retries) j["max_connection_retries"] = *max_connection_retries;
    if (retry_delay) j["retry_delay"] = retry_delay->count();
    if (enable_auto_reconnect) j["enable_auto_reconnect"] = *enable_auto_reconnect;
    if (default_buffer_count) j["default_buffer_count"] = *default_buffer_count;
    if (max_buffer_count) j["max_buffer_count"] = *max_buffer_count;
    if (capture_timeout) j["capture_timeout"] = capture_timeout->count();
    if (enable_performance_monitoring) j["enable_performance_monitoring"] = *enable_performance_monitoring;
    if (performance_log_interval) j["performance_log_interval"] = performance_log_interval->count();
    if (max_concurrent_captures) j["max_concurrent_captures"] = *max_concurrent_captures;
    
    return j;
}

// =============================================================================
// CAMERA CONFIG IMPLEMENTATION
// =============================================================================

core::VoidResult CameraConfig::validate() const {
    if (camera_id.get().empty()) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
            "Camera ID cannot be empty"));
    }
    
    if (priority && *priority > 100) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
            "Camera priority cannot exceed 100"));
    }
    
    return core::Ok();
}

core::Result<CameraConfig> CameraConfig::from_json(const nlohmann::json& j) {
    CameraConfig config{core::CameraId{""}};
    
    try {
        if (j.contains("camera_id")) {
            config.camera_id = core::CameraId{j["camera_id"].get<std::string>()};
        }
        if (j.contains("display_name")) {
            config.display_name = j["display_name"].get<std::string>();
        }
        if (j.contains("auto_connect")) {
            config.auto_connect = j["auto_connect"].get<bool>();
        }
        if (j.contains("priority")) {
            config.priority = j["priority"].get<uint32_t>();
        }
        if (j.contains("custom_parameters")) {
            config.custom_parameters = j["custom_parameters"].get<std::map<std::string, std::string>>();
        }
        
        if (auto result = config.validate(); !result.has_value()) {
            return core::Err<CameraConfig>(result.error());
        }
        
        return core::Ok(config);
    } catch (const nlohmann::json::exception& e) {
        return core::Err<CameraConfig>(core::make_error(core::ErrorCode::InvalidConfiguration,
            "JSON parsing error in CameraConfig: " + std::string(e.what())));
    }
}

nlohmann::json CameraConfig::to_json() const {
    nlohmann::json j;
    
    j["camera_id"] = camera_id.get();
    if (display_name) j["display_name"] = *display_name;
    if (auto_connect) j["auto_connect"] = *auto_connect;
    if (priority) j["priority"] = *priority;
    if (custom_parameters) j["custom_parameters"] = *custom_parameters;
    
    return j;
}

// =============================================================================
// LOGGING CONFIG IMPLEMENTATION
// =============================================================================

core::VoidResult LoggingConfig::validate() const {
    if (log_level) {
        const std::set<std::string> valid_levels = {"trace", "debug", "info", "warning", "error", "critical", "off"};
        if (valid_levels.find(*log_level) == valid_levels.end()) {
            return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
                "Invalid log level: " + *log_level));
        }
    }
    
    if (log_directory && log_directory->empty()) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
            "Log directory cannot be empty"));
    }
    
    if (log_filename && log_filename->empty()) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
            "Log filename cannot be empty"));
    }
    
    if (max_file_size && *max_file_size < 1024 * 1024) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
            "Max file size must be at least 1MB"));
    }
    
    if (max_files && *max_files == 0) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
            "Max files must be at least 1"));
    }
    
    return core::Ok();
}

core::Result<LoggingConfig> LoggingConfig::from_json(const nlohmann::json& j) {
    LoggingConfig config;
    
    try {
        if (j.contains("log_level")) {
            config.log_level = j["log_level"].get<std::string>();
        }
        if (j.contains("enable_console_logging")) {
            config.enable_console_logging = j["enable_console_logging"].get<bool>();
        }
        if (j.contains("enable_file_logging")) {
            config.enable_file_logging = j["enable_file_logging"].get<bool>();
        }
        if (j.contains("log_directory")) {
            config.log_directory = j["log_directory"].get<std::string>();
        }
        if (j.contains("log_filename")) {
            config.log_filename = j["log_filename"].get<std::string>();
        }
        if (j.contains("max_file_size")) {
            config.max_file_size = j["max_file_size"].get<size_t>();
        }
        if (j.contains("max_files")) {
            config.max_files = j["max_files"].get<size_t>();
        }
        if (j.contains("enable_structured_logging")) {
            config.enable_structured_logging = j["enable_structured_logging"].get<bool>();
        }
        if (j.contains("enable_performance_logging")) {
            config.enable_performance_logging = j["enable_performance_logging"].get<bool>();
        }
        
        if (auto result = config.validate(); !result.has_value()) {
            return core::Err<LoggingConfig>(result.error());
        }
        
        return core::Ok(config);
    } catch (const nlohmann::json::exception& e) {
        return core::Err<LoggingConfig>(core::make_error(core::ErrorCode::InvalidConfiguration,
            "JSON parsing error in LoggingConfig: " + std::string(e.what())));
    }
}

nlohmann::json LoggingConfig::to_json() const {
    nlohmann::json j;
    
    if (log_level) j["log_level"] = *log_level;
    if (enable_console_logging) j["enable_console_logging"] = *enable_console_logging;
    if (enable_file_logging) j["enable_file_logging"] = *enable_file_logging;
    if (log_directory) j["log_directory"] = *log_directory;
    if (log_filename) j["log_filename"] = *log_filename;
    if (max_file_size) j["max_file_size"] = *max_file_size;
    if (max_files) j["max_files"] = *max_files;
    if (enable_structured_logging) j["enable_structured_logging"] = *enable_structured_logging;
    if (enable_performance_logging) j["enable_performance_logging"] = *enable_performance_logging;
    
    return j;
}

// =============================================================================
// APPLICATION CONFIG IMPLEMENTATION
// =============================================================================

core::VoidResult ApplicationConfig::validate() const {
    if (auto result = camera_system.validate(); !result.has_value()) {
        return result;
    }
    
    if (auto result = logging.validate(); !result.has_value()) {
        return result;
    }
    
    for (const auto& camera : cameras) {
        if (auto result = camera.validate(); !result.has_value()) {
            return result;
        }
    }
    
    if (web_port && (*web_port < 1024 || *web_port > 65535)) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
            "Web port must be between 1024 and 65535"));
    }
    
    return core::Ok();
}

core::Result<ApplicationConfig> ApplicationConfig::from_json(const nlohmann::json& j) {
    ApplicationConfig config;
    
    try {
        if (j.contains("application_name")) {
            config.application_name = j["application_name"].get<std::string>();
        }
        if (j.contains("version")) {
            config.version = j["version"].get<std::string>();
        }
        if (j.contains("enable_web_interface")) {
            config.enable_web_interface = j["enable_web_interface"].get<bool>();
        }
        if (j.contains("web_port")) {
            config.web_port = j["web_port"].get<uint16_t>();
        }
        
        if (j.contains("camera_system")) {
            if (auto result = CameraSystemConfig::from_json(j["camera_system"]); result.has_value()) {
                config.camera_system = result.value();
            } else {
                return core::Err<ApplicationConfig>(result.error());
            }
        }
        
        if (j.contains("logging")) {
            if (auto result = LoggingConfig::from_json(j["logging"]); result.has_value()) {
                config.logging = result.value();
            } else {
                return core::Err<ApplicationConfig>(result.error());
            }
        }
        
        if (j.contains("cameras") && j["cameras"].is_array()) {
            for (const auto& camera_json : j["cameras"]) {
                if (auto result = CameraConfig::from_json(camera_json); result.has_value()) {
                    config.cameras.push_back(result.value());
                } else {
                    return core::Err<ApplicationConfig>(result.error());
                }
            }
        }
        
        if (j.contains("custom_settings")) {
            config.custom_settings = j["custom_settings"].get<std::map<std::string, nlohmann::json>>();
        }
        
        if (auto result = config.validate(); !result.has_value()) {
            return core::Err<ApplicationConfig>(result.error());
        }
        
        return core::Ok(config);
    } catch (const nlohmann::json::exception& e) {
        return core::Err<ApplicationConfig>(core::make_error(core::ErrorCode::InvalidConfiguration,
            "JSON parsing error in ApplicationConfig: " + std::string(e.what())));
    }
}

nlohmann::json ApplicationConfig::to_json() const {
    nlohmann::json j;
    
    if (application_name) j["application_name"] = *application_name;
    if (version) j["version"] = *version;
    if (enable_web_interface) j["enable_web_interface"] = *enable_web_interface;
    if (web_port) j["web_port"] = *web_port;
    
    j["camera_system"] = camera_system.to_json();
    j["logging"] = logging.to_json();
    
    if (!cameras.empty()) {
        j["cameras"] = nlohmann::json::array();
        for (const auto& camera : cameras) {
            j["cameras"].push_back(camera.to_json());
        }
    }
    
    if (!custom_settings.empty()) {
        j["custom_settings"] = custom_settings;
    }
    
    return j;
}

// =============================================================================
// CONFIGURATION MANAGER IMPLEMENTATION
// =============================================================================

ConfigurationManager::ConfigurationManager(std::filesystem::path config_path)
    : config_path_(std::move(config_path))
{
}

core::Result<ApplicationConfig> ConfigurationManager::load_configuration() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    if (!std::filesystem::exists(config_path_)) {
        // Create default configuration if file doesn't exist
        auto default_config = create_default_configuration();
        if (auto result = save_configuration(default_config); !result.has_value()) {
            return core::Err<ApplicationConfig>(result.error());
        }
        loaded_config_ = default_config;
        return core::Ok(default_config);
    }
    
    try {
        std::ifstream file(config_path_);
        if (!file.is_open()) {
            return core::Err<ApplicationConfig>(core::make_error(core::ErrorCode::FileReadError,
                "Failed to open configuration file: " + config_path_.string()));
        }
        
        nlohmann::json j;
        file >> j;
        
        auto result = ApplicationConfig::from_json(j);
        if (result.has_value()) {
            loaded_config_ = result.value();
        }
        
        return result;
    } catch (const nlohmann::json::exception& e) {
        return core::Err<ApplicationConfig>(core::make_error(core::ErrorCode::InvalidConfiguration,
            "JSON parsing error: " + std::string(e.what())));
    } catch (const std::exception& e) {
        return core::Err<ApplicationConfig>(core::make_error(core::ErrorCode::FileReadError,
            "Error reading configuration file: " + std::string(e.what())));
    }
}

core::VoidResult ConfigurationManager::save_configuration(const ApplicationConfig& config) {
    if (auto result = config.validate(); !result.has_value()) {
        return result;
    }
    
    if (auto result = ensure_config_directory(); !result.has_value()) {
        return result;
    }
    
    try {
        std::ofstream file(config_path_);
        if (!file.is_open()) {
            return core::Err(core::make_error(core::ErrorCode::FileWriteError,
                "Failed to open configuration file for writing: " + config_path_.string()));
        }
        
        nlohmann::json j = config.to_json();
        file << j.dump(4);
        
        return core::Ok();
    } catch (const std::exception& e) {
        return core::Err(core::make_error(core::ErrorCode::FileWriteError,
            "Error writing configuration file: " + std::string(e.what())));
    }
}

core::Result<ApplicationConfig> ConfigurationManager::get_configuration() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    if (!loaded_config_) {
        return load_configuration();
    }
    
    return core::Ok(*loaded_config_);
}

core::VoidResult ConfigurationManager::update_configuration(const ApplicationConfig& config) {
    if (auto result = save_configuration(config); !result.has_value()) {
        return result;
    }
    
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        loaded_config_ = config;
    }
    
    notify_listeners(config);
    return core::Ok();
}

void ConfigurationManager::add_change_listener(std::function<void(const ApplicationConfig&)> listener) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    change_listeners_.push_back(std::move(listener));
}

core::VoidResult ConfigurationManager::reload_configuration() {
    auto result = load_configuration();
    if (result.has_value()) {
        notify_listeners(result.value());
        return core::Ok();
    }
    return core::Err(result.error());
}

ApplicationConfig ConfigurationManager::create_default_configuration() {
    ApplicationConfig config;
    
    // Use default values from the struct definitions
    config.application_name = "SaperaCapture Pro";
    config.version = "2.0.0";
    config.enable_web_interface = false;
    config.web_port = 8080;
    
    // Camera system defaults are already set in the struct
    // Logging defaults are already set in the struct
    
    return config;
}

core::VoidResult ConfigurationManager::validate_configuration_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return core::Err(core::make_error(core::ErrorCode::FileNotFound,
            "Configuration file not found: " + path.string()));
    }
    
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return core::Err(core::make_error(core::ErrorCode::FileReadError,
                "Failed to open configuration file: " + path.string()));
        }
        
        nlohmann::json j;
        file >> j;
        
        auto result = ApplicationConfig::from_json(j);
        if (!result.has_value()) {
            return core::Err(result.error());
        }
        
        return core::Ok();
    } catch (const nlohmann::json::exception& e) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
            "JSON parsing error: " + std::string(e.what())));
    } catch (const std::exception& e) {
        return core::Err(core::make_error(core::ErrorCode::FileReadError,
            "Error reading configuration file: " + std::string(e.what())));
    }
}

bool ConfigurationManager::is_loaded() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return loaded_config_.has_value();
}

void ConfigurationManager::notify_listeners(const ApplicationConfig& config) {
    for (auto& listener : change_listeners_) {
        try {
            listener(config);
        } catch (const std::exception& e) {
            // Log error but continue with other listeners
            std::cerr << "Configuration listener error: " << e.what() << std::endl;
        }
    }
}

core::VoidResult ConfigurationManager::ensure_config_directory() const {
    try {
        if (auto parent = config_path_.parent_path(); !parent.empty()) {
            std::filesystem::create_directories(parent);
        }
        return core::Ok();
    } catch (const std::exception& e) {
        return core::Err(core::make_error(core::ErrorCode::FileWriteError,
            "Failed to create configuration directory: " + std::string(e.what())));
    }
}

// =============================================================================
// VALIDATION HELPERS IMPLEMENTATION
// =============================================================================

core::VoidResult validate_non_empty(std::string_view value, std::string_view field_name) {
    if (value.empty()) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
            std::format("Field '{}' cannot be empty", field_name)));
    }
    return core::Ok();
}

core::VoidResult validate_path_exists(const std::filesystem::path& path, std::string_view field_name) {
    if (!std::filesystem::exists(path)) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
            std::format("Path '{}' for field '{}' does not exist", path.string(), field_name)));
    }
    return core::Ok();
}

core::VoidResult validate_directory_writable(const std::filesystem::path& path, std::string_view field_name) {
    try {
        std::filesystem::create_directories(path);
        
        // Try to create a temporary file to test writability
        auto temp_file = path / "temp_test_file.tmp";
        std::ofstream test_file(temp_file);
        if (!test_file.is_open()) {
            return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
                std::format("Directory '{}' for field '{}' is not writable", path.string(), field_name)));
        }
        test_file.close();
        std::filesystem::remove(temp_file);
        
        return core::Ok();
    } catch (const std::exception& e) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
            std::format("Directory '{}' for field '{}' is not accessible: {}", 
                path.string(), field_name, e.what())));
    }
}

core::VoidResult validate_port(uint16_t port, std::string_view field_name) {
    if (port < 1024 || port > 65535) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
            std::format("Port {} for field '{}' must be between 1024 and 65535", port, field_name)));
    }
    return core::Ok();
}

core::VoidResult validate_timeout(std::chrono::milliseconds timeout,
                                 std::chrono::milliseconds min_timeout,
                                 std::chrono::milliseconds max_timeout,
                                 std::string_view field_name) {
    if (timeout < min_timeout || timeout > max_timeout) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
            std::format("Timeout {}ms for field '{}' must be between {}ms and {}ms",
                timeout.count(), field_name, min_timeout.count(), max_timeout.count())));
    }
    return core::Ok();
}

// =============================================================================
// ENVIRONMENT VARIABLE SUPPORT
// =============================================================================

std::optional<std::string> get_env_variable(std::string_view name) {
    const char* value = std::getenv(std::string(name).c_str());
    if (value && strlen(value) > 0) {
        return std::string(value);
    }
    return std::nullopt;
}

core::VoidResult apply_environment_overrides(ApplicationConfig& config) {
    // Apply environment variable overrides
    if (auto env_log_level = get_env_variable("SAPERA_LOG_LEVEL")) {
        config.logging.log_level = *env_log_level;
    }
    
    if (auto env_log_dir = get_env_variable("SAPERA_LOG_DIR")) {
        config.logging.log_directory = *env_log_dir;
    }
    
    if (auto env_web_port = get_env_variable("SAPERA_WEB_PORT")) {
        try {
            config.web_port = static_cast<uint16_t>(std::stoi(*env_web_port));
        } catch (const std::exception&) {
            return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
                "Invalid SAPERA_WEB_PORT environment variable"));
        }
    }
    
    return core::Ok();
}

// =============================================================================
// GLOBAL CONFIGURATION ACCESS
// =============================================================================

ConfigurationManager& get_global_config_manager() {
    static ConfigurationManager instance;
    return instance;
}

core::VoidResult initialize_configuration(const std::filesystem::path& config_path) {
    auto& manager = get_global_config_manager();
    manager = ConfigurationManager(config_path);
    
    // Load configuration to validate it
    auto result = manager.load_configuration();
    if (!result.has_value()) {
        return core::Err(result.error());
    }
    
    // Apply environment overrides
    auto config = result.value();
    if (auto env_result = apply_environment_overrides(config); !env_result.has_value()) {
        return env_result;
    }
    
    // Update configuration with environment overrides
    return manager.update_configuration(config);
}

core::Result<ApplicationConfig> get_application_config() {
    return get_global_config_manager().get_configuration();
}

void shutdown_configuration() {
    // Configuration manager will clean up automatically
}

} // namespace sapera::utils 