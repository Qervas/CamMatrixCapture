/**
 * Configuration.hpp - Modern Type-Safe Configuration System
 * JSON-based configuration with validation and type safety
 */

#pragma once

#include "../core/Types.hpp"
#include "../core/Result.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <optional>
#include <filesystem>
#include <concepts>
#include <type_traits>

namespace sapera::utils {

// =============================================================================
// CONFIGURATION CONCEPTS - Type constraints for configurations
// =============================================================================

template<typename T>
concept Configurable = requires(T t) {
    { t.validate() } -> std::same_as<core::VoidResult>;
    // Optional: { T::from_json(nlohmann::json{}) } -> std::same_as<core::Result<T>>;
    // Optional: { t.to_json() } -> std::same_as<nlohmann::json>;
};

template<typename T>
concept JsonSerializable = requires(T t, nlohmann::json j) {
    { T::from_json(j) } -> std::same_as<core::Result<T>>;
    { t.to_json() } -> std::same_as<nlohmann::json>;
};

// =============================================================================
// CAMERA SYSTEM CONFIGURATION
// =============================================================================

struct CameraSystemConfig {
    // Discovery settings
    std::optional<std::chrono::milliseconds> discovery_timeout = std::chrono::milliseconds{5000};
    std::optional<bool> auto_refresh_camera_list = true;
    std::optional<std::chrono::seconds> refresh_interval = std::chrono::seconds{30};
    
    // Connection settings
    std::optional<std::chrono::milliseconds> connection_timeout = std::chrono::milliseconds{10000};
    std::optional<uint32_t> max_connection_retries = 3;
    std::optional<std::chrono::milliseconds> retry_delay = std::chrono::milliseconds{1000};
    std::optional<bool> enable_auto_reconnect = true;
    
    // Capture settings
    std::optional<core::CaptureSettings> default_capture_settings;
    std::optional<uint32_t> default_buffer_count = 3;
    std::optional<uint32_t> max_buffer_count = 10;
    std::optional<std::chrono::milliseconds> capture_timeout = std::chrono::milliseconds{5000};
    
    // Performance settings
    std::optional<bool> enable_performance_monitoring = true;
    std::optional<std::chrono::seconds> performance_log_interval = std::chrono::seconds{60};
    std::optional<uint32_t> max_concurrent_captures = 4;
    
    // Validation
    [[nodiscard]] core::VoidResult validate() const;
    
    // JSON serialization
    [[nodiscard]] static core::Result<CameraSystemConfig> from_json(const nlohmann::json& j);
    [[nodiscard]] nlohmann::json to_json() const;
};

struct CameraConfig {
    core::CameraId camera_id;
    std::optional<std::string> display_name;
    std::optional<core::CaptureSettings> capture_settings;
    std::optional<bool> auto_connect = false;
    std::optional<uint32_t> priority = 0;
    std::optional<std::map<std::string, std::string>> custom_parameters;
    
    [[nodiscard]] core::VoidResult validate() const;
    [[nodiscard]] static core::Result<CameraConfig> from_json(const nlohmann::json& j);
    [[nodiscard]] nlohmann::json to_json() const;
};

struct LoggingConfig {
    std::optional<std::string> log_level = "info";
    std::optional<bool> enable_console_logging = true;
    std::optional<bool> enable_file_logging = true;
    std::optional<std::string> log_directory = "logs";
    std::optional<std::string> log_filename = "sapera_camera";
    std::optional<size_t> max_file_size = 10 * 1024 * 1024; // 10MB
    std::optional<size_t> max_files = 5;
    std::optional<bool> enable_structured_logging = true;
    std::optional<bool> enable_performance_logging = true;
    
    [[nodiscard]] core::VoidResult validate() const;
    [[nodiscard]] static core::Result<LoggingConfig> from_json(const nlohmann::json& j);
    [[nodiscard]] nlohmann::json to_json() const;
};

struct ApplicationConfig {
    // System settings
    std::optional<std::string> application_name = "SaperaCapture Pro";
    std::optional<std::string> version = "2.0.0";
    std::optional<bool> enable_web_interface = false;
    std::optional<uint16_t> web_port = 8080;
    
    // Component configurations
    CameraSystemConfig camera_system;
    LoggingConfig logging;
    std::vector<CameraConfig> cameras;
    
    // Custom settings
    std::map<std::string, nlohmann::json> custom_settings;
    
    [[nodiscard]] core::VoidResult validate() const;
    [[nodiscard]] static core::Result<ApplicationConfig> from_json(const nlohmann::json& j);
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// CONFIGURATION MANAGER
// =============================================================================

class ConfigurationManager {
private:
    std::filesystem::path config_path_;
    std::optional<ApplicationConfig> loaded_config_;
    mutable std::mutex config_mutex_;
    std::vector<std::function<void(const ApplicationConfig&)>> change_listeners_;
    
public:
    explicit ConfigurationManager(std::filesystem::path config_path = "config/application.json");
    
    /// Load configuration from file
    [[nodiscard]] core::Result<ApplicationConfig> load_configuration();
    
    /// Save configuration to file
    [[nodiscard]] core::VoidResult save_configuration(const ApplicationConfig& config);
    
    /// Get current configuration (loads if not already loaded)
    [[nodiscard]] core::Result<ApplicationConfig> get_configuration();
    
    /// Update configuration
    [[nodiscard]] core::VoidResult update_configuration(const ApplicationConfig& config);
    
    /// Get specific configuration section
    template<typename T>
    [[nodiscard]] core::Result<T> get_section(std::string_view section_name) const;
    
    /// Update specific configuration section
    template<typename T>
    [[nodiscard]] core::VoidResult update_section(std::string_view section_name, const T& section);
    
    /// Watch for configuration changes
    void add_change_listener(std::function<void(const ApplicationConfig&)> listener);
    
    /// Reload configuration from file
    [[nodiscard]] core::VoidResult reload_configuration();
    
    /// Create default configuration
    [[nodiscard]] static ApplicationConfig create_default_configuration();
    
    /// Validate configuration file
    [[nodiscard]] core::VoidResult validate_configuration_file(const std::filesystem::path& path);
    
    /// Get configuration file path
    [[nodiscard]] const std::filesystem::path& get_config_path() const { return config_path_; }
    
    /// Check if configuration is loaded
    [[nodiscard]] bool is_loaded() const;
    
private:
    void notify_listeners(const ApplicationConfig& config);
    [[nodiscard]] core::VoidResult ensure_config_directory() const;
};

// =============================================================================
// CONFIGURATION HELPERS
// =============================================================================

/// Load configuration from JSON file
template<Configurable T>
[[nodiscard]] core::Result<T> load_config_from_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return core::Err<T>(core::make_error(core::ErrorCode::FileNotFound, 
            "Configuration file not found: " + path.string()));
    }
    
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return core::Err<T>(core::make_error(core::ErrorCode::FileReadError, 
                "Failed to open configuration file: " + path.string()));
        }
        
        nlohmann::json j;
        file >> j;
        
        if constexpr (JsonSerializable<T>) {
            return T::from_json(j);
        } else {
            return core::Err<T>(core::make_error(core::ErrorCode::InvalidConfiguration, 
                "Type does not support JSON deserialization"));
        }
    } catch (const nlohmann::json::exception& e) {
        return core::Err<T>(core::make_error(core::ErrorCode::InvalidConfiguration, 
            "JSON parsing error: " + std::string(e.what())));
    } catch (const std::exception& e) {
        return core::Err<T>(core::make_error(core::ErrorCode::FileReadError, 
            "Error reading configuration file: " + std::string(e.what())));
    }
}

/// Save configuration to JSON file
template<Configurable T>
[[nodiscard]] core::VoidResult save_config_to_file(const T& config, const std::filesystem::path& path) {
    // Validate configuration first
    if (auto result = config.validate(); !result.has_value()) {
        return result;
    }
    
    try {
        // Ensure directory exists
        if (auto parent = path.parent_path(); !parent.empty()) {
            std::filesystem::create_directories(parent);
        }
        
        std::ofstream file(path);
        if (!file.is_open()) {
            return core::Err(core::make_error(core::ErrorCode::FileWriteError, 
                "Failed to open file for writing: " + path.string()));
        }
        
        if constexpr (JsonSerializable<T>) {
            nlohmann::json j = config.to_json();
            file << j.dump(4); // Pretty print with 4-space indentation
        } else {
            return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration, 
                "Type does not support JSON serialization"));
        }
        
        return core::Ok();
    } catch (const std::exception& e) {
        return core::Err(core::make_error(core::ErrorCode::FileWriteError, 
            "Error writing configuration file: " + std::string(e.what())));
    }
}

/// Merge two configurations (second overrides first where present)
template<typename T>
[[nodiscard]] T merge_configurations(const T& base, const T& override) {
    // This would need to be specialized for each configuration type
    // For now, just return the override
    return override;
}

/// Get configuration value with fallback
template<typename T>
[[nodiscard]] T get_value_or_default(const std::optional<T>& opt, const T& default_value) {
    return opt.value_or(default_value);
}

// =============================================================================
// CONFIGURATION VALIDATION HELPERS
// =============================================================================

/// Validate that a value is within a range
template<typename T>
[[nodiscard]] core::VoidResult validate_range(const T& value, const T& min, const T& max, 
                                             std::string_view field_name) {
    if (value < min || value > max) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration, 
            std::format("Field '{}' value {} is out of range [{}, {}]", 
                field_name, value, min, max)));
    }
    return core::Ok();
}

/// Validate that a string is not empty
[[nodiscard]] core::VoidResult validate_non_empty(std::string_view value, std::string_view field_name);

/// Validate that a path exists
[[nodiscard]] core::VoidResult validate_path_exists(const std::filesystem::path& path, std::string_view field_name);

/// Validate that a directory can be created
[[nodiscard]] core::VoidResult validate_directory_writable(const std::filesystem::path& path, std::string_view field_name);

/// Validate that a port number is valid
[[nodiscard]] core::VoidResult validate_port(uint16_t port, std::string_view field_name);

/// Validate timeout values
[[nodiscard]] core::VoidResult validate_timeout(std::chrono::milliseconds timeout, 
                                               std::chrono::milliseconds min_timeout,
                                               std::chrono::milliseconds max_timeout,
                                               std::string_view field_name);

// =============================================================================
// ENVIRONMENT VARIABLE SUPPORT
// =============================================================================

/// Get environment variable with optional default
[[nodiscard]] std::optional<std::string> get_env_variable(std::string_view name);

/// Get environment variable as specific type
template<typename T>
[[nodiscard]] std::optional<T> get_env_variable_as(std::string_view name);

/// Override configuration with environment variables
[[nodiscard]] core::VoidResult apply_environment_overrides(ApplicationConfig& config);

// =============================================================================
// CONFIGURATION SCHEMA VALIDATION
// =============================================================================

/// JSON schema validator for configuration files
class ConfigurationValidator {
private:
    nlohmann::json schema_;
    
public:
    explicit ConfigurationValidator(nlohmann::json schema);
    
    /// Validate JSON against schema
    [[nodiscard]] core::VoidResult validate(const nlohmann::json& config) const;
    
    /// Load schema from file
    [[nodiscard]] static core::Result<ConfigurationValidator> from_schema_file(const std::filesystem::path& schema_path);
    
    /// Get default schema for ApplicationConfig
    [[nodiscard]] static nlohmann::json get_default_schema();
};

// =============================================================================
// GLOBAL CONFIGURATION ACCESS
// =============================================================================

/// Get global configuration manager instance
[[nodiscard]] ConfigurationManager& get_global_config_manager();

/// Initialize global configuration
[[nodiscard]] core::VoidResult initialize_configuration(const std::filesystem::path& config_path);

/// Get current application configuration
[[nodiscard]] core::Result<ApplicationConfig> get_application_config();

/// Shutdown configuration system
void shutdown_configuration();

} // namespace sapera::utils 