/**
 * ApplicationBuilder.cpp - Application Builder and Main Application Implementation
 */

#include "Application.hpp"
#include <iostream>
#include <thread>

namespace sapera::application {

// =============================================================================
// APPLICATION BUILDER IMPLEMENTATION
// =============================================================================

ApplicationBuilder::ApplicationBuilder()
    : container_(std::make_unique<ServiceContainer>())
{
}

ApplicationBuilder& ApplicationBuilder::with_config_file(const std::filesystem::path& config_path) {
    try {
        auto config_manager = std::make_shared<utils::ConfigurationManager>(config_path);
        auto config_result = config_manager->load_configuration();
        
        if (config_result.has_value()) {
            config_ = config_result.value();
            container_->register_singleton(config_manager);
        } else {
            std::cerr << "Failed to load configuration: " << config_result.error().message << std::endl;
            config_ = create_default_app_config();
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception loading configuration: " << e.what() << std::endl;
        config_ = create_default_app_config();
    }
    
    return *this;
}

ApplicationBuilder& ApplicationBuilder::with_config(const utils::ApplicationConfig& config) {
    config_ = config;
    return *this;
}

ApplicationBuilder& ApplicationBuilder::with_logging(const utils::LoggerConfig& log_config) {
    // Convert logging config to application config
    if (config_.logging.log_level) *config_.logging.log_level = "info"; // Default
    config_.logging.enable_console_logging = log_config.enable_console;
    config_.logging.enable_file_logging = log_config.enable_file;
    config_.logging.log_directory = log_config.log_directory;
    config_.logging.log_filename = log_config.log_filename;
    config_.logging.max_file_size = log_config.max_file_size;
    config_.logging.max_files = log_config.max_files;
    
    return *this;
}

ApplicationBuilder& ApplicationBuilder::enable_web_interface(bool enabled) {
    config_.enable_web_interface = enabled;
    return *this;
}

ApplicationBuilder& ApplicationBuilder::enable_performance_monitoring(bool enabled) {
    config_.camera_system.enable_performance_monitoring = enabled;
    return *this;
}

core::Result<std::unique_ptr<Application>> ApplicationBuilder::build() {
    try {
        // Setup all services
        setup_logging();
        setup_configuration();
        setup_reactive_system();
        setup_hardware();
        setup_services();
        
        // Create application
        auto app = std::make_unique<Application>(std::move(container_));
        
        return core::Ok(std::move(app));
        
    } catch (const std::exception& e) {
        return core::Err<std::unique_ptr<Application>>(
            core::make_error(core::ErrorCode::ApplicationInitializationError,
                "Failed to build application: " + std::string(e.what())));
    }
}

void ApplicationBuilder::setup_logging() {
    // Create logger configuration from application config
    utils::LoggerConfig logger_config;
    
    if (config_.logging.log_level) {
        if (*config_.logging.log_level == "trace") logger_config.console_level = utils::LogLevel::Trace;
        else if (*config_.logging.log_level == "debug") logger_config.console_level = utils::LogLevel::Debug;
        else if (*config_.logging.log_level == "info") logger_config.console_level = utils::LogLevel::Info;
        else if (*config_.logging.log_level == "warning") logger_config.console_level = utils::LogLevel::Warning;
        else if (*config_.logging.log_level == "error") logger_config.console_level = utils::LogLevel::Error;
        else logger_config.console_level = utils::LogLevel::Info;
    }
    
    if (config_.logging.enable_console_logging) {
        logger_config.enable_console = *config_.logging.enable_console_logging;
    }
    
    if (config_.logging.enable_file_logging) {
        logger_config.enable_file = *config_.logging.enable_file_logging;
    }
    
    if (config_.logging.log_directory) {
        logger_config.log_directory = *config_.logging.log_directory;
    }
    
    if (config_.logging.log_filename) {
        logger_config.log_filename = *config_.logging.log_filename;
    }
    
    if (config_.logging.max_file_size) {
        logger_config.max_file_size = *config_.logging.max_file_size;
    }
    
    if (config_.logging.max_files) {
        logger_config.max_files = *config_.logging.max_files;
    }
    
    // Initialize logging system
    auto init_result = utils::initialize_logging(logger_config);
    if (!init_result.has_value()) {
        throw std::runtime_error("Failed to initialize logging: " + init_result.error().message);
    }
    
    // Create main logger
    logger_ = utils::get_logger("application");
    container_->register_singleton(logger_);
}

void ApplicationBuilder::setup_configuration() {
    // Create configuration manager if not already present
    if (!container_->has_service<utils::ConfigurationManager>()) {
        auto config_manager = std::make_shared<utils::ConfigurationManager>();
        
        // Save current configuration
        auto save_result = config_manager->save_configuration(config_);
        if (!save_result.has_value()) {
            logger_->warning("Failed to save configuration: {}", save_result.error().message);
        }
        
        container_->register_singleton(config_manager);
    }
}

void ApplicationBuilder::setup_reactive_system() {
    // Create event bus
    auto event_bus = std::make_shared<reactive::EventBus>();
    container_->register_singleton(event_bus);
    
    // Create event publishers for different event types
    auto camera_event_publisher = std::make_shared<reactive::EventPublisher<reactive::CameraEvent>>();
    container_->register_singleton(camera_event_publisher);
}

void ApplicationBuilder::setup_hardware() {
    // Create Sapera system manager
    auto sapera_manager = std::make_shared<hardware::SaperaSystemManager>(logger_);
    container_->register_singleton(sapera_manager);
}

void ApplicationBuilder::setup_services() {
    // Register factory for ApplicationService
    container_->register_factory<ApplicationService>([this]() {
        auto logger = container_->get<utils::Logger>();
        auto sapera_manager = container_->get<hardware::SaperaSystemManager>();
        auto event_bus = container_->get<reactive::EventBus>();
        auto config_manager = container_->get<utils::ConfigurationManager>();
        
        return std::make_shared<ApplicationService>(logger, sapera_manager, event_bus, config_manager);
    });
    
    // Register factory for CLIInterface
    container_->register_factory<CLIInterface>([this]() {
        auto app_service = container_->get<ApplicationService>();
        auto logger = container_->get<utils::Logger>();
        
        return std::make_shared<CLIInterface>(app_service, logger);
    });
}

// =============================================================================
// MAIN APPLICATION IMPLEMENTATION
// =============================================================================

Application::Application(std::unique_ptr<ServiceContainer> container)
    : container_(std::move(container))
    , start_time_(std::chrono::steady_clock::now())
{
    // Get core services
    logger_ = container_->get<utils::Logger>();
    
    if (!logger_) {
        throw std::runtime_error("Logger service not available");
    }
    
    logger_->debug("Created Application");
}

Application::~Application() {
    shutdown();
}

core::VoidResult Application::initialize() {
    if (is_initialized_.load()) {
        return core::Ok();
    }
    
    logger_->info("Initializing Application");
    
    try {
        // Print application banner
        print_application_banner(logger_);
        print_system_info(logger_);
        
        // Validate environment
        if (auto result = validate_environment(); !result.has_value()) {
            return result;
        }
        
        // Get application service
        app_service_ = container_->get<ApplicationService>();
        if (!app_service_) {
            return core::Err(core::make_error(core::ErrorCode::ApplicationInitializationError,
                "ApplicationService not available"));
        }
        
        // Initialize application service
        if (auto result = app_service_->initialize(); !result.has_value()) {
            return result;
        }
        
        // Get CLI interface
        cli_interface_ = container_->get<CLIInterface>();
        if (!cli_interface_) {
            return core::Err(core::make_error(core::ErrorCode::ApplicationInitializationError,
                "CLIInterface not available"));
        }
        
        // Initialize CLI
        cli_interface_->initialize();
        
        // Setup signal handlers
        setup_signal_handlers();
        
        // Start background services
        start_background_services();
        
        is_initialized_.store(true);
        logger_->info("Application initialized successfully");
        
        return core::Ok();
        
    } catch (const std::exception& e) {
        return core::Err(core::make_error(core::ErrorCode::ApplicationInitializationError,
            "Exception during initialization: " + std::string(e.what())));
    }
}

core::VoidResult Application::run() {
    if (!is_initialized()) {
        auto init_result = initialize();
        if (!init_result.has_value()) {
            return init_result;
        }
    }
    
    logger_->info("Running Application");
    
    // Start application service
    if (auto result = app_service_->start(); !result.has_value()) {
        return result;
    }
    
    logger_->info("Application started successfully. Use Ctrl+C to stop.");
    
    // Main application loop - just wait for shutdown signal
    while (app_service_->is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    logger_->info("Application run completed");
    return core::Ok();
}

core::VoidResult Application::run_interactive() {
    if (!is_initialized()) {
        auto init_result = initialize();
        if (!init_result.has_value()) {
            return init_result;
        }
    }
    
    logger_->info("Running Application in interactive mode");
    
    // Start application service
    if (auto result = app_service_->start(); !result.has_value()) {
        return result;
    }
    
    // Start interactive CLI
    cli_interface_->start_interactive_mode();
    
    logger_->info("Interactive mode completed");
    return core::Ok();
}

void Application::shutdown() {
    if (!is_initialized_.load()) {
        return;
    }
    
    logger_->info("Shutting down Application");
    
    // Stop background services
    stop_background_services();
    
    // Stop CLI interface
    if (cli_interface_) {
        cli_interface_->stop_interactive_mode();
    }
    
    // Shutdown application service
    if (app_service_) {
        app_service_->shutdown();
    }
    
    // Shutdown logging
    utils::shutdown_logging();
    
    is_initialized_.store(false);
    
    std::cout << "Application shutdown complete.\n";
}

std::chrono::milliseconds Application::get_uptime() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
}

void Application::setup_signal_handlers() {
    setup_signal_handlers(this);
}

void Application::start_background_services() {
    should_stop_monitoring_.store(false);
    monitoring_thread_ = std::make_unique<std::thread>(&Application::monitoring_loop, this);
    logger_->debug("Background services started");
}

void Application::stop_background_services() {
    should_stop_monitoring_.store(true);
    
    if (monitoring_thread_ && monitoring_thread_->joinable()) {
        monitoring_thread_->join();
    }
    
    monitoring_thread_.reset();
    logger_->debug("Background services stopped");
}

void Application::monitoring_loop() {
    logger_->debug("Started monitoring loop");
    
    while (!should_stop_monitoring_.load()) {
        try {
            log_system_health();
            
            // Sleep for 30 seconds between health checks
            for (int i = 0; i < 300 && !should_stop_monitoring_.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        } catch (const std::exception& e) {
            logger_->error("Exception in monitoring loop: {}", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    logger_->debug("Monitoring loop ended");
}

void Application::log_system_health() {
    if (!app_service_) return;
    
    try {
        // Get system status
        auto status_result = app_service_->get_system_status();
        if (status_result.has_value()) {
            const auto& status = status_result.value();
            
            auto context = utils::LogContext{}
                .add("event", "system_health")
                .add("is_running", status.is_running ? "true" : "false")
                .add("connected_cameras", status.connected_cameras)
                .add("uptime_seconds", std::chrono::duration_cast<std::chrono::seconds>(status.uptime).count());
            
            logger_->info_with_context(context, "System health check: {} cameras connected, running for {} seconds",
                status.connected_cameras, std::chrono::duration_cast<std::chrono::seconds>(status.uptime).count());
        }
        
        // Get system statistics
        auto stats_result = app_service_->get_system_statistics();
        if (stats_result.has_value()) {
            const auto& stats = stats_result.value();
            
            auto context = utils::LogContext{}
                .add("event", "system_statistics")
                .add("total_captures", stats.total_captures)
                .add("successful_captures", stats.successful_captures)
                .add("failed_captures", stats.failed_captures)
                .add("success_rate", stats.success_rate);
            
            if (stats.total_captures > 0) {
                logger_->info_with_context(context, "Capture statistics: {}/{} successful ({:.1f}%)",
                    stats.successful_captures, stats.total_captures, stats.success_rate * 100.0);
            }
        }
    } catch (const std::exception& e) {
        logger_->error("Exception during system health check: {}", e.what());
    }
}

} // namespace sapera::application 