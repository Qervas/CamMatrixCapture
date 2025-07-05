/**
 * Logger.hpp - Modern Structured Logging System
 * Built on spdlog with performance measurement and type-safe formatting
 */

#pragma once

#include "../core/Types.hpp"
#include "../core/Result.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/fmt/fmt.h>
#include <memory>
#include <string>
#include <string_view>
#include <chrono>
#include <source_location>
#include <unordered_map>
#include <mutex>

namespace sapera::utils {

// =============================================================================
// LOG LEVELS AND CONFIGURATION
// =============================================================================

enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Critical = 5,
    Off = 6
};

struct LoggerConfig {
    LogLevel console_level = LogLevel::Info;
    LogLevel file_level = LogLevel::Debug;
    std::string log_directory = "logs";
    std::string log_filename = "sapera_camera";
    size_t max_file_size = 10 * 1024 * 1024; // 10MB
    size_t max_files = 5;
    bool enable_console = true;
    bool enable_file = true;
    std::string log_pattern = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v";
    
    [[nodiscard]] core::VoidResult validate() const;
};

// =============================================================================
// PERFORMANCE MEASUREMENT
// =============================================================================

class ScopedTimer {
private:
    std::string operation_name_;
    std::chrono::high_resolution_clock::time_point start_time_;
    std::shared_ptr<spdlog::logger> logger_;
    
public:
    explicit ScopedTimer(std::string_view operation_name, 
                        std::shared_ptr<spdlog::logger> logger = nullptr);
    ~ScopedTimer();
    
    // Get elapsed time without destroying timer
    [[nodiscard]] std::chrono::milliseconds elapsed() const;
};

// Macro for easy performance measurement
#define MEASURE_SCOPE(name) \
    sapera::utils::ScopedTimer timer_##__LINE__(name, sapera::utils::get_logger())

#define MEASURE_SCOPE_WITH_LOGGER(name, logger) \
    sapera::utils::ScopedTimer timer_##__LINE__(name, logger)

// =============================================================================
// STRUCTURED LOGGING CONTEXT
// =============================================================================

struct LogContext {
    std::unordered_map<std::string, std::string> fields;
    
    LogContext& add(std::string_view key, std::string_view value) {
        fields[std::string(key)] = std::string(value);
        return *this;
    }
    
    template<typename T>
    LogContext& add(std::string_view key, const T& value) {
        fields[std::string(key)] = fmt::format("{}", value);
        return *this;
    }
    
    LogContext& add_camera_id(const core::CameraId& id) {
        return add("camera_id", id.get());
    }
    
    LogContext& add_serial_number(const core::SerialNumber& serial) {
        return add("serial_number", serial.get());
    }
    
    LogContext& add_error_code(core::ErrorCode code) {
        return add("error_code", static_cast<int>(code));
    }
    
    [[nodiscard]] std::string to_string() const;
};

// =============================================================================
// MODERN LOGGER CLASS
// =============================================================================

class Logger {
private:
    std::shared_ptr<spdlog::logger> logger_;
    LoggerConfig config_;
    mutable std::mutex context_mutex_;
    LogContext global_context_;
    
public:
    explicit Logger(std::string_view name, const LoggerConfig& config = {});
    ~Logger() = default;
    
    // Non-copyable but movable
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = default;
    Logger& operator=(Logger&&) = default;
    
    /// Configure logger
    [[nodiscard]] core::VoidResult configure(const LoggerConfig& config);
    
    /// Set global context that's added to all log messages
    void set_global_context(const LogContext& context);
    void add_global_context(std::string_view key, std::string_view value);
    
    /// Template-based type-safe logging
    template<typename... Args>
    void trace(std::string_view format, Args&&... args,
              std::source_location location = std::source_location::current()) {
        log_impl(LogLevel::Trace, format, location, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void debug(std::string_view format, Args&&... args,
              std::source_location location = std::source_location::current()) {
        log_impl(LogLevel::Debug, format, location, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void info(std::string_view format, Args&&... args,
             std::source_location location = std::source_location::current()) {
        log_impl(LogLevel::Info, format, location, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void warning(std::string_view format, Args&&... args,
                std::source_location location = std::source_location::current()) {
        log_impl(LogLevel::Warning, format, location, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void error(std::string_view format, Args&&... args,
              std::source_location location = std::source_location::current()) {
        log_impl(LogLevel::Error, format, location, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void critical(std::string_view format, Args&&... args,
                 std::source_location location = std::source_location::current()) {
        log_impl(LogLevel::Critical, format, location, std::forward<Args>(args)...);
    }
    
    /// Structured logging with context
    template<typename... Args>
    void info_with_context(const LogContext& context, std::string_view format, Args&&... args) {
        log_with_context_impl(LogLevel::Info, context, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void error_with_context(const LogContext& context, std::string_view format, Args&&... args) {
        log_with_context_impl(LogLevel::Error, context, format, std::forward<Args>(args)...);
    }
    
    /// Log camera operations
    void log_camera_connected(const core::CameraId& id, const core::SerialNumber& serial);
    void log_camera_disconnected(const core::CameraId& id, std::string_view reason = "");
    void log_camera_error(const core::CameraId& id, const core::Error& error);
    void log_image_captured(const core::CameraId& id, uint32_t width, uint32_t height, 
                           std::chrono::milliseconds capture_time);
    
    /// Log system events
    void log_system_startup();
    void log_system_shutdown();
    void log_performance_metrics(const std::string& operation, 
                               std::chrono::milliseconds duration);
    
    /// Error logging
    void log_error(const core::Error& error, const LogContext& context = {});
    void log_result_error(const std::string& operation, const core::Error& error);
    
    /// Get underlying spdlog logger
    [[nodiscard]] std::shared_ptr<spdlog::logger> get_spdlog_logger() const { return logger_; }
    
    /// Set log level
    void set_level(LogLevel level);
    [[nodiscard]] LogLevel get_level() const;
    
    /// Flush logs
    void flush();
    
private:
    template<typename... Args>
    void log_impl(LogLevel level, std::string_view format, 
                 const std::source_location& location, Args&&... args) {
        if (!should_log(level)) return;
        
        try {
            auto message = fmt::format(fmt::runtime(format), std::forward<Args>(args)...);
            auto full_message = add_location_info(message, location);
            
            std::lock_guard<std::mutex> lock(context_mutex_);
            if (!global_context_.fields.empty()) {
                full_message += " " + global_context_.to_string();
            }
            
            log_to_spdlog(level, full_message);
        } catch (const std::exception& e) {
            // Fallback logging if formatting fails
            logger_->error("Logging format error: {} | Original format: {}", e.what(), format);
        }
    }
    
    template<typename... Args>
    void log_with_context_impl(LogLevel level, const LogContext& context, 
                              std::string_view format, Args&&... args) {
        if (!should_log(level)) return;
        
        try {
            auto message = fmt::format(fmt::runtime(format), std::forward<Args>(args)...);
            auto full_message = message + " " + context.to_string();
            
            std::lock_guard<std::mutex> lock(context_mutex_);
            if (!global_context_.fields.empty()) {
                full_message += " " + global_context_.to_string();
            }
            
            log_to_spdlog(level, full_message);
        } catch (const std::exception& e) {
            logger_->error("Structured logging error: {} | Format: {}", e.what(), format);
        }
    }
    
    [[nodiscard]] bool should_log(LogLevel level) const;
    void log_to_spdlog(LogLevel level, const std::string& message);
    [[nodiscard]] std::string add_location_info(const std::string& message, 
                                               const std::source_location& location) const;
    [[nodiscard]] spdlog::level::level_enum to_spdlog_level(LogLevel level) const;
    [[nodiscard]] LogLevel from_spdlog_level(spdlog::level::level_enum level) const;
};

// =============================================================================
// GLOBAL LOGGER MANAGEMENT
// =============================================================================

class LoggerManager {
private:
    std::unordered_map<std::string, std::shared_ptr<Logger>> loggers_;
    std::mutex loggers_mutex_;
    LoggerConfig default_config_;
    
    LoggerManager() = default;
    
public:
    static LoggerManager& instance();
    
    /// Get or create logger
    [[nodiscard]] std::shared_ptr<Logger> get_logger(std::string_view name);
    
    /// Create logger with specific config
    [[nodiscard]] std::shared_ptr<Logger> create_logger(std::string_view name, 
                                                       const LoggerConfig& config);
    
    /// Set default configuration for new loggers
    void set_default_config(const LoggerConfig& config);
    
    /// Configure all existing loggers
    void configure_all(const LoggerConfig& config);
    
    /// Shutdown all loggers
    void shutdown();
    
    /// Get logger names
    [[nodiscard]] std::vector<std::string> get_logger_names() const;
    
    /// Flush all loggers
    void flush_all();
};

// =============================================================================
// CONVENIENCE FUNCTIONS
// =============================================================================

/// Get default logger
[[nodiscard]] std::shared_ptr<Logger> get_logger(std::string_view name = "sapera");

/// Get camera-specific logger
[[nodiscard]] std::shared_ptr<Logger> get_camera_logger(const core::CameraId& camera_id);

/// Initialize logging system
[[nodiscard]] core::VoidResult initialize_logging(const LoggerConfig& config = {});

/// Shutdown logging system
void shutdown_logging();

/// Create context for camera operations
[[nodiscard]] LogContext create_camera_context(const core::CameraId& id, 
                                              const core::SerialNumber& serial = core::SerialNumber{""});

/// Create context for error logging
[[nodiscard]] LogContext create_error_context(const core::Error& error);

// =============================================================================
// SPECIALIZED LOGGERS
// =============================================================================

/// Performance logger for benchmarking
class PerformanceLogger {
private:
    std::shared_ptr<Logger> logger_;
    std::unordered_map<std::string, std::vector<std::chrono::milliseconds>> measurements_;
    std::mutex measurements_mutex_;
    
public:
    explicit PerformanceLogger(std::shared_ptr<Logger> logger);
    
    void log_measurement(std::string_view operation, std::chrono::milliseconds duration);
    void log_statistics(std::string_view operation);
    void clear_measurements(std::string_view operation = "");
    
    [[nodiscard]] std::vector<std::chrono::milliseconds> get_measurements(std::string_view operation) const;
};

/// System health logger
class SystemHealthLogger {
private:
    std::shared_ptr<Logger> logger_;
    std::thread monitoring_thread_;
    std::atomic<bool> monitoring_active_{false};
    std::chrono::seconds monitoring_interval_{30};
    
public:
    explicit SystemHealthLogger(std::shared_ptr<Logger> logger);
    ~SystemHealthLogger();
    
    void start_monitoring(std::chrono::seconds interval = std::chrono::seconds{30});
    void stop_monitoring();
    void log_health_snapshot();
    
private:
    void monitoring_loop();
    [[nodiscard]] double get_cpu_usage() const;
    [[nodiscard]] double get_memory_usage() const;
};

} // namespace sapera::utils 