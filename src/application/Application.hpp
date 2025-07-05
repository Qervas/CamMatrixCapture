/**
 * Application.hpp - Modern Application Layer
 * Clean architecture with dependency injection and service management
 */

#pragma once

#include "../core/Types.hpp"
#include "../core/Result.hpp"
#include "../utils/Logger.hpp"
#include "../utils/Configuration.hpp"
#include "../hardware/SaperaCamera.hpp"
#include "../reactive/EventSystem.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

namespace sapera::application {

// =============================================================================
// SERVICE CONTAINER - Dependency Injection
// =============================================================================

class ServiceContainer {
private:
    std::map<std::string, std::shared_ptr<void>> services_;
    std::map<std::string, std::function<std::shared_ptr<void>()>> factories_;
    mutable std::mutex container_mutex_;
    
public:
    ServiceContainer() = default;
    ~ServiceContainer() = default;
    
    // Non-copyable
    ServiceContainer(const ServiceContainer&) = delete;
    ServiceContainer& operator=(const ServiceContainer&) = delete;
    
    /// Register a singleton service
    template<typename T>
    void register_singleton(std::shared_ptr<T> service) {
        std::lock_guard<std::mutex> lock(container_mutex_);
        services_[typeid(T).name()] = service;
    }
    
    /// Register a factory for creating services
    template<typename T>
    void register_factory(std::function<std::shared_ptr<T>()> factory) {
        std::lock_guard<std::mutex> lock(container_mutex_);
        factories_[typeid(T).name()] = [factory]() -> std::shared_ptr<void> {
            return std::static_pointer_cast<void>(factory());
        };
    }
    
    /// Get service instance
    template<typename T>
    [[nodiscard]] std::shared_ptr<T> get() {
        std::lock_guard<std::mutex> lock(container_mutex_);
        
        std::string type_name = typeid(T).name();
        
        // Check if singleton already exists
        auto service_it = services_.find(type_name);
        if (service_it != services_.end()) {
            return std::static_pointer_cast<T>(service_it->second);
        }
        
        // Check if factory exists
        auto factory_it = factories_.find(type_name);
        if (factory_it != factories_.end()) {
            auto service = factory_it->second();
            services_[type_name] = service; // Cache as singleton
            return std::static_pointer_cast<T>(service);
        }
        
        return nullptr;
    }
    
    /// Check if service is registered
    template<typename T>
    [[nodiscard]] bool has_service() const {
        std::lock_guard<std::mutex> lock(container_mutex_);
        std::string type_name = typeid(T).name();
        return services_.find(type_name) != services_.end() || 
               factories_.find(type_name) != factories_.end();
    }
    
    /// Clear all services
    void clear() {
        std::lock_guard<std::mutex> lock(container_mutex_);
        services_.clear();
        factories_.clear();
    }
};

// =============================================================================
// APPLICATION SERVICE - Core business logic
// =============================================================================

class ApplicationService {
private:
    std::shared_ptr<utils::Logger> logger_;
    std::shared_ptr<hardware::SaperaSystemManager> sapera_manager_;
    std::shared_ptr<reactive::EventBus> event_bus_;
    std::shared_ptr<utils::ConfigurationManager> config_manager_;
    
    // Application state
    std::atomic<bool> is_running_{false};
    std::map<core::CameraId, std::shared_ptr<hardware::SaperaCamera>> active_cameras_;
    mutable std::mutex cameras_mutex_;
    
    // Statistics
    std::atomic<uint64_t> total_captures_{0};
    std::atomic<uint64_t> successful_captures_{0};
    std::chrono::steady_clock::time_point start_time_;
    
public:
    explicit ApplicationService(std::shared_ptr<utils::Logger> logger,
                              std::shared_ptr<hardware::SaperaSystemManager> sapera_manager,
                              std::shared_ptr<reactive::EventBus> event_bus,
                              std::shared_ptr<utils::ConfigurationManager> config_manager);
    
    ~ApplicationService() = default;
    
    /// Initialize the application service
    [[nodiscard]] core::VoidResult initialize();
    
    /// Shutdown the application service
    void shutdown();
    
    /// Start the application
    [[nodiscard]] core::VoidResult start();
    
    /// Stop the application
    void stop();
    
    /// Check if application is running
    [[nodiscard]] bool is_running() const { return is_running_.load(); }
    
    // Camera management
    [[nodiscard]] core::AsyncResult<std::vector<core::CameraInfo>> discover_cameras();
    [[nodiscard]] core::AsyncResult<void> connect_camera(const core::CameraId& camera_id);
    [[nodiscard]] core::AsyncResult<void> disconnect_camera(const core::CameraId& camera_id);
    [[nodiscard]] core::AsyncResult<core::ImageData> capture_image(const core::CameraId& camera_id);
    [[nodiscard]] std::vector<core::CameraId> get_connected_cameras() const;
    
    // System operations
    [[nodiscard]] core::Result<core::SystemStatus> get_system_status() const;
    [[nodiscard]] core::Result<core::SystemStatistics> get_system_statistics() const;
    [[nodiscard]] core::AsyncResult<void> reset_system();
    
    // Configuration
    [[nodiscard]] core::Result<utils::ApplicationConfig> get_configuration() const;
    [[nodiscard]] core::VoidResult update_configuration(const utils::ApplicationConfig& config);
    
    // Event handling
    void subscribe_to_camera_events(std::function<void(const reactive::CameraEvent&)> handler);
    
private:
    void setup_event_handlers();
    void handle_camera_event(const reactive::CameraEvent& event);
    void update_capture_statistics(bool success);
};

// =============================================================================
// CLI INTERFACE - Command line interface
// =============================================================================

struct CLICommand {
    std::string name;
    std::string description;
    std::function<core::VoidResult(const std::vector<std::string>&)> handler;
    std::vector<std::string> required_args;
    std::vector<std::string> optional_args;
};

class CLIInterface {
private:
    std::shared_ptr<ApplicationService> app_service_;
    std::shared_ptr<utils::Logger> logger_;
    std::map<std::string, CLICommand> commands_;
    std::atomic<bool> is_interactive_{false};
    
public:
    explicit CLIInterface(std::shared_ptr<ApplicationService> app_service,
                         std::shared_ptr<utils::Logger> logger);
    
    /// Initialize CLI with available commands
    void initialize();
    
    /// Execute a command with arguments
    [[nodiscard]] core::VoidResult execute_command(const std::string& command_line);
    
    /// Start interactive mode
    void start_interactive_mode();
    
    /// Stop interactive mode
    void stop_interactive_mode();
    
    /// Print help for all commands
    void print_help() const;
    
    /// Print help for specific command
    void print_command_help(const std::string& command_name) const;
    
private:
    void register_commands();
    [[nodiscard]] std::pair<std::string, std::vector<std::string>> parse_command_line(const std::string& command_line) const;
    
    // Command implementations
    [[nodiscard]] core::VoidResult cmd_list_cameras(const std::vector<std::string>& args);
    [[nodiscard]] core::VoidResult cmd_connect_camera(const std::vector<std::string>& args);
    [[nodiscard]] core::VoidResult cmd_disconnect_camera(const std::vector<std::string>& args);
    [[nodiscard]] core::VoidResult cmd_capture_image(const std::vector<std::string>& args);
    [[nodiscard]] core::VoidResult cmd_system_status(const std::vector<std::string>& args);
    [[nodiscard]] core::VoidResult cmd_system_stats(const std::vector<std::string>& args);
    [[nodiscard]] core::VoidResult cmd_reset_system(const std::vector<std::string>& args);
    [[nodiscard]] core::VoidResult cmd_help(const std::vector<std::string>& args);
    [[nodiscard]] core::VoidResult cmd_exit(const std::vector<std::string>& args);
};

// =============================================================================
// APPLICATION BUILDER - Factory for creating configured applications
// =============================================================================

class ApplicationBuilder {
private:
    std::unique_ptr<ServiceContainer> container_;
    utils::ApplicationConfig config_;
    std::shared_ptr<utils::Logger> logger_;
    
public:
    ApplicationBuilder();
    ~ApplicationBuilder() = default;
    
    /// Load configuration from file
    [[nodiscard]] ApplicationBuilder& with_config_file(const std::filesystem::path& config_path);
    
    /// Use provided configuration
    [[nodiscard]] ApplicationBuilder& with_config(const utils::ApplicationConfig& config);
    
    /// Configure logging
    [[nodiscard]] ApplicationBuilder& with_logging(const utils::LoggerConfig& log_config);
    
    /// Enable/disable specific features
    [[nodiscard]] ApplicationBuilder& enable_web_interface(bool enabled = true);
    [[nodiscard]] ApplicationBuilder& enable_performance_monitoring(bool enabled = true);
    
    /// Build the complete application
    [[nodiscard]] core::Result<std::unique_ptr<Application>> build();
    
private:
    void setup_logging();
    void setup_services();
    void setup_configuration();
    void setup_hardware();
    void setup_reactive_system();
};

// =============================================================================
// MAIN APPLICATION CLASS
// =============================================================================

class Application {
private:
    std::unique_ptr<ServiceContainer> container_;
    std::shared_ptr<ApplicationService> app_service_;
    std::shared_ptr<CLIInterface> cli_interface_;
    std::shared_ptr<utils::Logger> logger_;
    
    // Background services
    std::unique_ptr<std::thread> monitoring_thread_;
    std::atomic<bool> should_stop_monitoring_{false};
    
    // Application state
    std::atomic<bool> is_initialized_{false};
    std::chrono::steady_clock::time_point start_time_;
    
public:
    explicit Application(std::unique_ptr<ServiceContainer> container);
    ~Application();
    
    // Non-copyable, non-movable
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;
    
    /// Initialize the application
    [[nodiscard]] core::VoidResult initialize();
    
    /// Run the application
    [[nodiscard]] core::VoidResult run();
    
    /// Run in interactive CLI mode
    [[nodiscard]] core::VoidResult run_interactive();
    
    /// Shutdown the application
    void shutdown();
    
    /// Get application service
    [[nodiscard]] std::shared_ptr<ApplicationService> get_app_service() const { return app_service_; }
    
    /// Get CLI interface
    [[nodiscard]] std::shared_ptr<CLIInterface> get_cli_interface() const { return cli_interface_; }
    
    /// Get service container
    [[nodiscard]] ServiceContainer* get_container() const { return container_.get(); }
    
    /// Check if application is initialized
    [[nodiscard]] bool is_initialized() const { return is_initialized_.load(); }
    
    /// Get application uptime
    [[nodiscard]] std::chrono::milliseconds get_uptime() const;
    
private:
    void setup_signal_handlers();
    void start_background_services();
    void stop_background_services();
    void monitoring_loop();
    void log_system_health();
};

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/// Create default application configuration
[[nodiscard]] utils::ApplicationConfig create_default_app_config();

/// Setup signal handlers for graceful shutdown
void setup_signal_handlers(Application* app);

/// Print application banner
void print_application_banner(std::shared_ptr<utils::Logger> logger);

/// Print system information
void print_system_info(std::shared_ptr<utils::Logger> logger);

/// Validate application environment
[[nodiscard]] core::VoidResult validate_environment();

} // namespace sapera::application 