/**
 * Logger.cpp - Modern Structured Logging Implementation
 */

#include "Logger.hpp"
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace sapera::utils {

// =============================================================================
// LOGGER CONFIG VALIDATION
// =============================================================================

core::VoidResult LoggerConfig::validate() const {
    if (log_directory.empty()) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration, 
            "Log directory cannot be empty"));
    }
    
    if (log_filename.empty()) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration, 
            "Log filename cannot be empty"));
    }
    
    if (max_file_size == 0) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration, 
            "Max file size must be greater than 0"));
    }
    
    if (max_files == 0) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration, 
            "Max files must be greater than 0"));
    }
    
    return core::Ok();
}

// =============================================================================
// SCOPED TIMER IMPLEMENTATION
// =============================================================================

ScopedTimer::ScopedTimer(std::string_view operation_name, std::shared_ptr<spdlog::logger> logger)
    : operation_name_(operation_name)
    , start_time_(std::chrono::high_resolution_clock::now())
    , logger_(logger)
{
    if (logger_) {
        logger_->debug("Started operation: {}", operation_name_);
    }
}

ScopedTimer::~ScopedTimer() {
    auto duration = elapsed();
    if (logger_) {
        logger_->info("Operation '{}' completed in {}ms", operation_name_, duration.count());
    }
}

std::chrono::milliseconds ScopedTimer::elapsed() const {
    auto end_time = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time_);
}

// =============================================================================
// LOG CONTEXT IMPLEMENTATION
// =============================================================================

std::string LogContext::to_string() const {
    if (fields.empty()) return "";
    
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (const auto& [key, value] : fields) {
        if (!first) oss << ", ";
        oss << key << "=" << value;
        first = false;
    }
    oss << "]";
    return oss.str();
}

// =============================================================================
// LOGGER IMPLEMENTATION
// =============================================================================

Logger::Logger(std::string_view name, const LoggerConfig& config)
    : config_(config)
{
    // Validate configuration
    if (auto result = config_.validate(); !result.has_value()) {
        throw std::runtime_error("Invalid logger configuration: " + result.error().message);
    }
    
    // Create sinks based on configuration
    std::vector<spdlog::sink_ptr> sinks;
    
    if (config_.enable_console) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(to_spdlog_level(config_.console_level));
        console_sink->set_pattern(config_.log_pattern);
        sinks.push_back(console_sink);
    }
    
    if (config_.enable_file) {
        // Create log directory if it doesn't exist
        std::filesystem::create_directories(config_.log_directory);
        
        auto log_path = std::filesystem::path(config_.log_directory) / 
                       (config_.log_filename + ".log");
        
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_path.string(), config_.max_file_size, config_.max_files);
        file_sink->set_level(to_spdlog_level(config_.file_level));
        file_sink->set_pattern(config_.log_pattern);
        sinks.push_back(file_sink);
    }
    
    // Create logger
    logger_ = std::make_shared<spdlog::logger>(std::string(name), sinks.begin(), sinks.end());
    logger_->set_level(spdlog::level::trace); // Let sinks handle filtering
    logger_->flush_on(spdlog::level::warn);
    
    // Register with spdlog
    spdlog::register_logger(logger_);
}

core::VoidResult Logger::configure(const LoggerConfig& config) {
    if (auto result = config.validate(); !result.has_value()) {
        return result;
    }
    
    config_ = config;
    
    // Update sink levels
    for (auto& sink : logger_->sinks()) {
        // This is a simplified approach - in practice you'd need to identify sink types
        sink->set_level(to_spdlog_level(config_.console_level));
        sink->set_pattern(config_.log_pattern);
    }
    
    return core::Ok();
}

void Logger::set_global_context(const LogContext& context) {
    std::lock_guard<std::mutex> lock(context_mutex_);
    global_context_ = context;
}

void Logger::add_global_context(std::string_view key, std::string_view value) {
    std::lock_guard<std::mutex> lock(context_mutex_);
    global_context_.add(key, value);
}

void Logger::log_camera_connected(const core::CameraId& id, const core::SerialNumber& serial) {
    auto context = LogContext{}
        .add_camera_id(id)
        .add_serial_number(serial)
        .add("event", "camera_connected");
    
    info_with_context(context, "Camera connected: {} ({})", id.get(), serial.get());
}

void Logger::log_camera_disconnected(const core::CameraId& id, std::string_view reason) {
    auto context = LogContext{}
        .add_camera_id(id)
        .add("event", "camera_disconnected");
    
    if (!reason.empty()) {
        context.add("reason", reason);
    }
    
    info_with_context(context, "Camera disconnected: {}", id.get());
}

void Logger::log_camera_error(const core::CameraId& id, const core::Error& error) {
    auto context = LogContext{}
        .add_camera_id(id)
        .add("event", "camera_error")
        .add_error_code(error.code);
    
    error_with_context(context, "Camera error: {} - {}", id.get(), error.message);
}

void Logger::log_image_captured(const core::CameraId& id, uint32_t width, uint32_t height, 
                               std::chrono::milliseconds capture_time) {
    auto context = LogContext{}
        .add_camera_id(id)
        .add("event", "image_captured")
        .add("width", width)
        .add("height", height)
        .add("capture_time_ms", capture_time.count());
    
    info_with_context(context, "Image captured: {} ({}x{} in {}ms)", 
        id.get(), width, height, capture_time.count());
}

void Logger::log_system_startup() {
    auto context = LogContext{}.add("event", "system_startup");
    info_with_context(context, "SaperaCapture Pro system starting up");
}

void Logger::log_system_shutdown() {
    auto context = LogContext{}.add("event", "system_shutdown");
    info_with_context(context, "SaperaCapture Pro system shutting down");
}

void Logger::log_performance_metrics(const std::string& operation, 
                                   std::chrono::milliseconds duration) {
    auto context = LogContext{}
        .add("event", "performance_metric")
        .add("operation", operation)
        .add("duration_ms", duration.count());
    
    info_with_context(context, "Performance: {} took {}ms", operation, duration.count());
}

void Logger::log_error(const core::Error& error, const LogContext& context) {
    auto error_context = context;
    error_context.add_error_code(error.code)
                 .add("error_message", error.message);
    
    if (!error.details.empty()) {
        error_context.add("error_details", error.details);
    }
    
    if (error.context) {
        error_context.add("error_context", *error.context);
    }
    
    error_with_context(error_context, "Error occurred: {}", error.message);
}

void Logger::log_result_error(const std::string& operation, const core::Error& error) {
    auto context = LogContext{}
        .add("operation", operation)
        .add_error_code(error.code);
    
    log_error(error, context);
}

void Logger::set_level(LogLevel level) {
    logger_->set_level(to_spdlog_level(level));
}

LogLevel Logger::get_level() const {
    return from_spdlog_level(logger_->level());
}

void Logger::flush() {
    logger_->flush();
}

bool Logger::should_log(LogLevel level) const {
    return static_cast<int>(level) >= static_cast<int>(get_level());
}

void Logger::log_to_spdlog(LogLevel level, const std::string& message) {
    switch (level) {
        case LogLevel::Trace:
            logger_->trace(message);
            break;
        case LogLevel::Debug:
            logger_->debug(message);
            break;
        case LogLevel::Info:
            logger_->info(message);
            break;
        case LogLevel::Warning:
            logger_->warn(message);
            break;
        case LogLevel::Error:
            logger_->error(message);
            break;
        case LogLevel::Critical:
            logger_->critical(message);
            break;
        case LogLevel::Off:
            break;
    }
}

std::string Logger::add_location_info(const std::string& message, 
                                     const std::source_location& location) const {
    return fmt::format("{} [{}:{}]", message, 
        std::filesystem::path(location.file_name()).filename().string(), 
        location.line());
}

spdlog::level::level_enum Logger::to_spdlog_level(LogLevel level) const {
    switch (level) {
        case LogLevel::Trace: return spdlog::level::trace;
        case LogLevel::Debug: return spdlog::level::debug;
        case LogLevel::Info: return spdlog::level::info;
        case LogLevel::Warning: return spdlog::level::warn;
        case LogLevel::Error: return spdlog::level::err;
        case LogLevel::Critical: return spdlog::level::critical;
        case LogLevel::Off: return spdlog::level::off;
        default: return spdlog::level::info;
    }
}

LogLevel Logger::from_spdlog_level(spdlog::level::level_enum level) const {
    switch (level) {
        case spdlog::level::trace: return LogLevel::Trace;
        case spdlog::level::debug: return LogLevel::Debug;
        case spdlog::level::info: return LogLevel::Info;
        case spdlog::level::warn: return LogLevel::Warning;
        case spdlog::level::err: return LogLevel::Error;
        case spdlog::level::critical: return LogLevel::Critical;
        case spdlog::level::off: return LogLevel::Off;
        default: return LogLevel::Info;
    }
}

// =============================================================================
// LOGGER MANAGER IMPLEMENTATION
// =============================================================================

LoggerManager& LoggerManager::instance() {
    static LoggerManager instance;
    return instance;
}

std::shared_ptr<Logger> LoggerManager::get_logger(std::string_view name) {
    std::lock_guard<std::mutex> lock(loggers_mutex_);
    
    std::string name_str(name);
    auto it = loggers_.find(name_str);
    if (it != loggers_.end()) {
        return it->second;
    }
    
    // Create new logger with default config
    auto logger = std::make_shared<Logger>(name, default_config_);
    loggers_[name_str] = logger;
    return logger;
}

std::shared_ptr<Logger> LoggerManager::create_logger(std::string_view name, 
                                                   const LoggerConfig& config) {
    std::lock_guard<std::mutex> lock(loggers_mutex_);
    
    std::string name_str(name);
    auto logger = std::make_shared<Logger>(name, config);
    loggers_[name_str] = logger;
    return logger;
}

void LoggerManager::set_default_config(const LoggerConfig& config) {
    std::lock_guard<std::mutex> lock(loggers_mutex_);
    default_config_ = config;
}

void LoggerManager::configure_all(const LoggerConfig& config) {
    std::lock_guard<std::mutex> lock(loggers_mutex_);
    
    default_config_ = config;
    for (auto& [name, logger] : loggers_) {
        logger->configure(config);
    }
}

void LoggerManager::shutdown() {
    std::lock_guard<std::mutex> lock(loggers_mutex_);
    
    for (auto& [name, logger] : loggers_) {
        logger->flush();
    }
    
    loggers_.clear();
    spdlog::shutdown();
}

std::vector<std::string> LoggerManager::get_logger_names() const {
    std::lock_guard<std::mutex> lock(loggers_mutex_);
    
    std::vector<std::string> names;
    names.reserve(loggers_.size());
    for (const auto& [name, logger] : loggers_) {
        names.push_back(name);
    }
    return names;
}

void LoggerManager::flush_all() {
    std::lock_guard<std::mutex> lock(loggers_mutex_);
    
    for (auto& [name, logger] : loggers_) {
        logger->flush();
    }
}

// =============================================================================
// CONVENIENCE FUNCTIONS
// =============================================================================

std::shared_ptr<Logger> get_logger(std::string_view name) {
    return LoggerManager::instance().get_logger(name);
}

std::shared_ptr<Logger> get_camera_logger(const core::CameraId& camera_id) {
    auto name = "camera_" + camera_id.get();
    return LoggerManager::instance().get_logger(name);
}

core::VoidResult initialize_logging(const LoggerConfig& config) {
    if (auto result = config.validate(); !result.has_value()) {
        return result;
    }
    
    LoggerManager::instance().set_default_config(config);
    
    // Create default logger
    auto default_logger = LoggerManager::instance().get_logger("sapera");
    default_logger->log_system_startup();
    
    return core::Ok();
}

void shutdown_logging() {
    auto default_logger = get_logger("sapera");
    default_logger->log_system_shutdown();
    
    LoggerManager::instance().shutdown();
}

LogContext create_camera_context(const core::CameraId& id, const core::SerialNumber& serial) {
    auto context = LogContext{}.add_camera_id(id);
    if (!serial.get().empty()) {
        context.add_serial_number(serial);
    }
    return context;
}

LogContext create_error_context(const core::Error& error) {
    auto context = LogContext{}
        .add_error_code(error.code)
        .add("error_message", error.message);
    
    if (!error.details.empty()) {
        context.add("error_details", error.details);
    }
    
    if (error.context) {
        context.add("error_context", *error.context);
    }
    
    return context;
}

// =============================================================================
// PERFORMANCE LOGGER IMPLEMENTATION
// =============================================================================

PerformanceLogger::PerformanceLogger(std::shared_ptr<Logger> logger)
    : logger_(logger)
{
}

void PerformanceLogger::log_measurement(std::string_view operation, std::chrono::milliseconds duration) {
    std::lock_guard<std::mutex> lock(measurements_mutex_);
    measurements_[std::string(operation)].push_back(duration);
    
    logger_->log_performance_metrics(std::string(operation), duration);
}

void PerformanceLogger::log_statistics(std::string_view operation) {
    std::lock_guard<std::mutex> lock(measurements_mutex_);
    
    auto it = measurements_.find(std::string(operation));
    if (it == measurements_.end() || it->second.empty()) {
        logger_->warning("No measurements found for operation: {}", operation);
        return;
    }
    
    const auto& measurements = it->second;
    
    // Calculate statistics
    auto total = std::chrono::milliseconds{0};
    auto min_duration = measurements[0];
    auto max_duration = measurements[0];
    
    for (const auto& duration : measurements) {
        total += duration;
        min_duration = std::min(min_duration, duration);
        max_duration = std::max(max_duration, duration);
    }
    
    auto average = total / measurements.size();
    
    auto context = LogContext{}
        .add("operation", operation)
        .add("count", measurements.size())
        .add("average_ms", average.count())
        .add("min_ms", min_duration.count())
        .add("max_ms", max_duration.count())
        .add("total_ms", total.count());
    
    logger_->info_with_context(context, 
        "Performance statistics for '{}': {} measurements, avg={}ms, min={}ms, max={}ms",
        operation, measurements.size(), average.count(), min_duration.count(), max_duration.count());
}

void PerformanceLogger::clear_measurements(std::string_view operation) {
    std::lock_guard<std::mutex> lock(measurements_mutex_);
    
    if (operation.empty()) {
        measurements_.clear();
        logger_->info("Cleared all performance measurements");
    } else {
        measurements_.erase(std::string(operation));
        logger_->info("Cleared performance measurements for operation: {}", operation);
    }
}

std::vector<std::chrono::milliseconds> PerformanceLogger::get_measurements(std::string_view operation) const {
    std::lock_guard<std::mutex> lock(measurements_mutex_);
    
    auto it = measurements_.find(std::string(operation));
    if (it != measurements_.end()) {
        return it->second;
    }
    return {};
}

// =============================================================================
// SYSTEM HEALTH LOGGER IMPLEMENTATION
// =============================================================================

SystemHealthLogger::SystemHealthLogger(std::shared_ptr<Logger> logger)
    : logger_(logger)
{
}

SystemHealthLogger::~SystemHealthLogger() {
    stop_monitoring();
}

void SystemHealthLogger::start_monitoring(std::chrono::seconds interval) {
    if (monitoring_active_) {
        return;
    }
    
    monitoring_interval_ = interval;
    monitoring_active_ = true;
    monitoring_thread_ = std::thread(&SystemHealthLogger::monitoring_loop, this);
    
    logger_->info("Started system health monitoring (interval: {}s)", interval.count());
}

void SystemHealthLogger::stop_monitoring() {
    if (!monitoring_active_) {
        return;
    }
    
    monitoring_active_ = false;
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
    
    logger_->info("Stopped system health monitoring");
}

void SystemHealthLogger::log_health_snapshot() {
    auto cpu_usage = get_cpu_usage();
    auto memory_usage = get_memory_usage();
    
    auto context = LogContext{}
        .add("event", "system_health")
        .add("cpu_usage_percent", cpu_usage)
        .add("memory_usage_percent", memory_usage)
        .add("thread_id", std::this_thread::get_id());
    
    logger_->info_with_context(context, 
        "System health: CPU={:.1f}%, Memory={:.1f}%", 
        cpu_usage, memory_usage);
}

void SystemHealthLogger::monitoring_loop() {
    while (monitoring_active_) {
        log_health_snapshot();
        std::this_thread::sleep_for(monitoring_interval_);
    }
}

double SystemHealthLogger::get_cpu_usage() const {
    // Simplified CPU usage - in production you'd want more accurate measurement
#ifdef _WIN32
    // Windows implementation would go here
    return 0.0; // Placeholder
#else
    // Linux implementation would go here
    return 0.0; // Placeholder
#endif
}

double SystemHealthLogger::get_memory_usage() const {
    // Simplified memory usage - in production you'd want more accurate measurement
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0); // MB
    }
#else
    // Linux implementation would go here
#endif
    return 0.0; // Placeholder
}

} // namespace sapera::utils 