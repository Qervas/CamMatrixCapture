/**
 * Application.cpp - Modern Application Layer Implementation
 */

#include "Application.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <future>
#include <signal.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/utsname.h>
#endif

namespace sapera::application {

// Global application pointer for signal handling
static Application* g_application = nullptr;

// =============================================================================
// APPLICATION SERVICE IMPLEMENTATION
// =============================================================================

ApplicationService::ApplicationService(std::shared_ptr<utils::Logger> logger,
                                     std::shared_ptr<hardware::SaperaSystemManager> sapera_manager,
                                     std::shared_ptr<reactive::EventBus> event_bus,
                                     std::shared_ptr<utils::ConfigurationManager> config_manager)
    : logger_(logger)
    , sapera_manager_(sapera_manager)
    , event_bus_(event_bus)
    , config_manager_(config_manager)
    , start_time_(std::chrono::steady_clock::now())
{
    logger_->debug("Created ApplicationService");
}

core::VoidResult ApplicationService::initialize() {
    logger_->info("Initializing ApplicationService");
    
    // Initialize Sapera system
    if (auto result = sapera_manager_->initialize(); !result.has_value()) {
        return result;
    }
    
    // Setup event handlers
    setup_event_handlers();
    
    logger_->info("ApplicationService initialized successfully");
    return core::Ok();
}

void ApplicationService::shutdown() {
    logger_->info("Shutting down ApplicationService");
    
    stop();
    
    // Disconnect all cameras
    {
        std::lock_guard<std::mutex> lock(cameras_mutex_);
        for (auto& [camera_id, camera] : active_cameras_) {
            if (camera && camera->is_connected()) {
                camera->disconnect().wait();
            }
        }
        active_cameras_.clear();
    }
    
    // Shutdown Sapera system
    sapera_manager_->shutdown();
    
    logger_->info("ApplicationService shutdown complete");
}

core::VoidResult ApplicationService::start() {
    if (is_running_.load()) {
        return core::Ok();
    }
    
    logger_->info("Starting ApplicationService");
    
    is_running_.store(true);
    start_time_ = std::chrono::steady_clock::now();
    
    return core::Ok();
}

void ApplicationService::stop() {
    if (!is_running_.load()) {
        return;
    }
    
    logger_->info("Stopping ApplicationService");
    is_running_.store(false);
}

core::AsyncResult<std::vector<core::CameraInfo>> ApplicationService::discover_cameras() {
    return std::async(std::launch::async, [this]() -> core::Result<std::vector<core::CameraInfo>> {
        logger_->info("Discovering cameras");
        
        auto factory = sapera_manager_->get_camera_factory();
        if (!factory) {
            return core::Err<std::vector<core::CameraInfo>>(
                core::make_error(core::ErrorCode::CameraInitializationError, "Camera factory not available"));
        }
        
        auto result = factory->discover_cameras();
        auto status = result.wait_for(std::chrono::seconds(10));
        
        if (status == std::future_status::timeout) {
            return core::Err<std::vector<core::CameraInfo>>(
                core::make_error(core::ErrorCode::CameraDiscoveryError, "Camera discovery timeout"));
        }
        
        auto cameras = result.get();
        if (cameras.has_value()) {
            logger_->info("Discovered {} cameras", cameras.value().size());
        } else {
            logger_->error("Camera discovery failed: {}", cameras.error().message);
        }
        
        return cameras;
    });
}

core::AsyncResult<void> ApplicationService::connect_camera(const core::CameraId& camera_id) {
    return std::async(std::launch::async, [this, camera_id]() -> core::VoidResult {
        std::lock_guard<std::mutex> lock(cameras_mutex_);
        
        logger_->info("Connecting to camera {}", camera_id.get());
        
        // Check if already connected
        auto it = active_cameras_.find(camera_id);
        if (it != active_cameras_.end() && it->second->is_connected()) {
            logger_->debug("Camera {} already connected", camera_id.get());
            return core::Ok();
        }
        
        // Get or create camera
        auto camera_result = sapera_manager_->get_camera(camera_id);
        if (!camera_result.has_value()) {
            return core::Err(camera_result.error());
        }
        
        auto camera = camera_result.value();
        
        // Connect camera
        auto connect_result = camera->connect();
        auto status = connect_result.wait_for(std::chrono::seconds(15));
        
        if (status == std::future_status::timeout) {
            return core::Err(core::make_error(core::ErrorCode::CameraConnectionError,
                "Camera connection timeout"));
        }
        
        auto result = connect_result.get();
        if (result.has_value()) {
            active_cameras_[camera_id] = std::shared_ptr<hardware::SaperaCamera>(camera);
            logger_->info("Successfully connected to camera {}", camera_id.get());
        } else {
            logger_->error("Failed to connect to camera {}: {}", camera_id.get(), result.error().message);
        }
        
        return result;
    });
}

core::AsyncResult<void> ApplicationService::disconnect_camera(const core::CameraId& camera_id) {
    return std::async(std::launch::async, [this, camera_id]() -> core::VoidResult {
        std::lock_guard<std::mutex> lock(cameras_mutex_);
        
        logger_->info("Disconnecting camera {}", camera_id.get());
        
        auto it = active_cameras_.find(camera_id);
        if (it == active_cameras_.end()) {
            logger_->debug("Camera {} not found in active cameras", camera_id.get());
            return core::Ok();
        }
        
        auto camera = it->second;
        if (!camera->is_connected()) {
            logger_->debug("Camera {} already disconnected", camera_id.get());
            active_cameras_.erase(it);
            return core::Ok();
        }
        
        // Disconnect camera
        auto disconnect_result = camera->disconnect();
        auto status = disconnect_result.wait_for(std::chrono::seconds(10));
        
        if (status == std::future_status::timeout) {
            logger_->warning("Camera {} disconnect timeout", camera_id.get());
        }
        
        auto result = disconnect_result.get();
        active_cameras_.erase(it);
        
        if (result.has_value()) {
            logger_->info("Successfully disconnected camera {}", camera_id.get());
        } else {
            logger_->error("Failed to disconnect camera {}: {}", camera_id.get(), result.error().message);
        }
        
        return result;
    });
}

core::AsyncResult<core::ImageData> ApplicationService::capture_image(const core::CameraId& camera_id) {
    return std::async(std::launch::async, [this, camera_id]() -> core::Result<core::ImageData> {
        std::lock_guard<std::mutex> lock(cameras_mutex_);
        
        logger_->debug("Capturing image from camera {}", camera_id.get());
        
        auto it = active_cameras_.find(camera_id);
        if (it == active_cameras_.end()) {
            return core::Err<core::ImageData>(core::make_error(core::ErrorCode::CameraNotFound,
                "Camera not found in active cameras: " + camera_id.get()));
        }
        
        auto camera = it->second;
        if (!camera->is_connected()) {
            return core::Err<core::ImageData>(core::make_error(core::ErrorCode::CameraNotConnected,
                "Camera not connected: " + camera_id.get()));
        }
        
        total_captures_++;
        
        // Capture image
        auto capture_result = camera->capture_image();
        auto status = capture_result.wait_for(std::chrono::seconds(10));
        
        if (status == std::future_status::timeout) {
            update_capture_statistics(false);
            return core::Err<core::ImageData>(core::make_error(core::ErrorCode::CaptureTimeout,
                "Image capture timeout"));
        }
        
        auto result = capture_result.get();
        update_capture_statistics(result.has_value());
        
        if (result.has_value()) {
            logger_->debug("Successfully captured image from camera {}", camera_id.get());
        } else {
            logger_->error("Failed to capture image from camera {}: {}", camera_id.get(), result.error().message);
        }
        
        return result;
    });
}

std::vector<core::CameraId> ApplicationService::get_connected_cameras() const {
    std::lock_guard<std::mutex> lock(cameras_mutex_);
    
    std::vector<core::CameraId> connected_cameras;
    for (const auto& [camera_id, camera] : active_cameras_) {
        if (camera && camera->is_connected()) {
            connected_cameras.push_back(camera_id);
        }
    }
    
    return connected_cameras;
}

core::Result<core::SystemStatus> ApplicationService::get_system_status() const {
    core::SystemStatus status;
    status.is_running = is_running_.load();
    status.uptime = std::chrono::steady_clock::now() - start_time_;
    status.connected_cameras = get_connected_cameras().size();
    status.sapera_initialized = sapera_manager_->is_initialized();
    status.last_update = std::chrono::steady_clock::now();
    
    return core::Ok(status);
}

core::Result<core::SystemStatistics> ApplicationService::get_system_statistics() const {
    core::SystemStatistics stats;
    stats.total_captures = total_captures_.load();
    stats.successful_captures = successful_captures_.load();
    stats.failed_captures = total_captures_.load() - successful_captures_.load();
    stats.uptime = std::chrono::steady_clock::now() - start_time_;
    
    if (stats.total_captures > 0) {
        stats.success_rate = static_cast<double>(stats.successful_captures) / stats.total_captures;
    } else {
        stats.success_rate = 0.0;
    }
    
    return core::Ok(stats);
}

core::AsyncResult<void> ApplicationService::reset_system() {
    return std::async(std::launch::async, [this]() -> core::VoidResult {
        logger_->info("Resetting system");
        
        // Stop the application
        stop();
        
        // Disconnect all cameras
        auto connected_cameras = get_connected_cameras();
        for (const auto& camera_id : connected_cameras) {
            disconnect_camera(camera_id).wait();
        }
        
        // Reset statistics
        total_captures_.store(0);
        successful_captures_.store(0);
        
        // Restart the application
        return start();
    });
}

core::Result<utils::ApplicationConfig> ApplicationService::get_configuration() const {
    return config_manager_->get_configuration();
}

core::VoidResult ApplicationService::update_configuration(const utils::ApplicationConfig& config) {
    return config_manager_->update_configuration(config);
}

void ApplicationService::subscribe_to_camera_events(std::function<void(const reactive::CameraEvent&)> handler) {
    if (event_bus_) {
        event_bus_->subscribe<reactive::CameraEvent>(handler);
    }
}

void ApplicationService::setup_event_handlers() {
    subscribe_to_camera_events([this](const reactive::CameraEvent& event) {
        handle_camera_event(event);
    });
}

void ApplicationService::handle_camera_event(const reactive::CameraEvent& event) {
    // Handle different types of camera events
    if (auto connected_event = std::get_if<reactive::CameraConnectedEvent>(&event)) {
        logger_->info("Camera connected event: {}", connected_event->camera_id.get());
    } else if (auto disconnected_event = std::get_if<reactive::CameraDisconnectedEvent>(&event)) {
        logger_->info("Camera disconnected event: {}", disconnected_event->camera_id.get());
    } else if (auto error_event = std::get_if<reactive::CameraErrorEvent>(&event)) {
        logger_->error("Camera error event: {} - {}", error_event->camera_id.get(), error_event->error.message);
    } else if (auto capture_event = std::get_if<reactive::ImageCapturedEvent>(&event)) {
        logger_->debug("Image captured event: {} ({})", capture_event->camera_id.get(), capture_event->capture_id.get());
    }
}

void ApplicationService::update_capture_statistics(bool success) {
    if (success) {
        successful_captures_++;
    }
}

// =============================================================================
// CLI INTERFACE IMPLEMENTATION
// =============================================================================

CLIInterface::CLIInterface(std::shared_ptr<ApplicationService> app_service,
                          std::shared_ptr<utils::Logger> logger)
    : app_service_(app_service)
    , logger_(logger)
{
    logger_->debug("Created CLIInterface");
}

void CLIInterface::initialize() {
    logger_->info("Initializing CLI Interface");
    register_commands();
}

core::VoidResult CLIInterface::execute_command(const std::string& command_line) {
    if (command_line.empty()) {
        return core::Ok();
    }
    
    auto [command_name, args] = parse_command_line(command_line);
    
    auto it = commands_.find(command_name);
    if (it == commands_.end()) {
        logger_->error("Unknown command: {}", command_name);
        return core::Err(core::make_error(core::ErrorCode::InvalidCommand, 
            "Unknown command: " + command_name));
    }
    
    const auto& command = it->second;
    
    // Check required arguments
    if (args.size() < command.required_args.size()) {
        logger_->error("Insufficient arguments for command '{}'. Required: {}", 
            command_name, command.required_args.size());
        print_command_help(command_name);
        return core::Err(core::make_error(core::ErrorCode::InvalidArguments,
            "Insufficient arguments"));
    }
    
    try {
        return command.handler(args);
    } catch (const std::exception& e) {
        logger_->error("Exception executing command '{}': {}", command_name, e.what());
        return core::Err(core::make_error(core::ErrorCode::CommandExecutionError,
            "Command execution failed: " + std::string(e.what())));
    }
}

void CLIInterface::start_interactive_mode() {
    is_interactive_.store(true);
    logger_->info("Starting interactive CLI mode");
    
    std::cout << "\n";
    std::cout << "=== SaperaCapture Pro Interactive Mode ===\n";
    std::cout << "Type 'help' for available commands, 'exit' to quit.\n";
    std::cout << "\n";
    
    std::string input;
    while (is_interactive_.load()) {
        std::cout << "sapera> ";
        std::getline(std::cin, input);
        
        if (std::cin.eof()) {
            break;
        }
        
        if (!input.empty()) {
            auto result = execute_command(input);
            if (!result.has_value()) {
                std::cout << "Error: " << result.error().message << "\n";
            }
        }
    }
    
    logger_->info("Interactive CLI mode ended");
}

void CLIInterface::stop_interactive_mode() {
    is_interactive_.store(false);
}

void CLIInterface::print_help() const {
    std::cout << "\nAvailable Commands:\n";
    std::cout << "==================\n";
    
    for (const auto& [name, command] : commands_) {
        std::cout << std::format("  {:15} - {}\n", name, command.description);
    }
    
    std::cout << "\nUse 'help <command>' for detailed help on a specific command.\n\n";
}

void CLIInterface::print_command_help(const std::string& command_name) const {
    auto it = commands_.find(command_name);
    if (it == commands_.end()) {
        std::cout << "Unknown command: " << command_name << "\n";
        return;
    }
    
    const auto& command = it->second;
    
    std::cout << "\nCommand: " << command_name << "\n";
    std::cout << "Description: " << command.description << "\n";
    
    if (!command.required_args.empty()) {
        std::cout << "Required arguments: ";
        for (size_t i = 0; i < command.required_args.size(); ++i) {
            std::cout << "<" << command.required_args[i] << ">";
            if (i < command.required_args.size() - 1) std::cout << " ";
        }
        std::cout << "\n";
    }
    
    if (!command.optional_args.empty()) {
        std::cout << "Optional arguments: ";
        for (size_t i = 0; i < command.optional_args.size(); ++i) {
            std::cout << "[" << command.optional_args[i] << "]";
            if (i < command.optional_args.size() - 1) std::cout << " ";
        }
        std::cout << "\n";
    }
    
    std::cout << "\n";
}

void CLIInterface::register_commands() {
    commands_["list"] = CLICommand{
        "list", "List all discovered cameras",
        [this](const auto& args) { return cmd_list_cameras(args); },
        {}, {}
    };
    
    commands_["connect"] = CLICommand{
        "connect", "Connect to a camera",
        [this](const auto& args) { return cmd_connect_camera(args); },
        {"camera_id"}, {}
    };
    
    commands_["disconnect"] = CLICommand{
        "disconnect", "Disconnect from a camera",
        [this](const auto& args) { return cmd_disconnect_camera(args); },
        {"camera_id"}, {}
    };
    
    commands_["capture"] = CLICommand{
        "capture", "Capture an image from a camera",
        [this](const auto& args) { return cmd_capture_image(args); },
        {"camera_id"}, {"output_file"}
    };
    
    commands_["status"] = CLICommand{
        "status", "Show system status",
        [this](const auto& args) { return cmd_system_status(args); },
        {}, {}
    };
    
    commands_["stats"] = CLICommand{
        "stats", "Show system statistics",
        [this](const auto& args) { return cmd_system_stats(args); },
        {}, {}
    };
    
    commands_["reset"] = CLICommand{
        "reset", "Reset the system",
        [this](const auto& args) { return cmd_reset_system(args); },
        {}, {}
    };
    
    commands_["help"] = CLICommand{
        "help", "Show help information",
        [this](const auto& args) { return cmd_help(args); },
        {}, {"command"}
    };
    
    commands_["exit"] = CLICommand{
        "exit", "Exit the application",
        [this](const auto& args) { return cmd_exit(args); },
        {}, {}
    };
}

std::pair<std::string, std::vector<std::string>> CLIInterface::parse_command_line(const std::string& command_line) const {
    std::istringstream iss(command_line);
    std::string command_name;
    iss >> command_name;
    
    std::vector<std::string> args;
    std::string arg;
    while (iss >> arg) {
        args.push_back(arg);
    }
    
    return {command_name, args};
}

// Command implementations
core::VoidResult CLIInterface::cmd_list_cameras(const std::vector<std::string>& args) {
    std::cout << "Discovering cameras...\n";
    
    auto result = app_service_->discover_cameras();
    auto cameras = result.get();
    
    if (!cameras.has_value()) {
        std::cout << "Error discovering cameras: " << cameras.error().message << "\n";
        return core::Err(cameras.error());
    }
    
    if (cameras.value().empty()) {
        std::cout << "No cameras found.\n";
    } else {
        std::cout << "\nDiscovered Cameras:\n";
        std::cout << "==================\n";
        for (const auto& camera : cameras.value()) {
            std::cout << std::format("  ID: {}\n", camera.camera_id.get());
            std::cout << std::format("  Serial: {}\n", camera.serial_number.get());
            std::cout << std::format("  Model: {}\n", camera.model_name);
            std::cout << std::format("  Status: {}\n", camera.is_connected ? "Connected" : "Disconnected");
            std::cout << "\n";
        }
    }
    
    return core::Ok();
}

core::VoidResult CLIInterface::cmd_connect_camera(const std::vector<std::string>& args) {
    core::CameraId camera_id{args[0]};
    
    std::cout << "Connecting to camera " << camera_id.get() << "...\n";
    
    auto result = app_service_->connect_camera(camera_id);
    auto connect_result = result.get();
    
    if (connect_result.has_value()) {
        std::cout << "Successfully connected to camera " << camera_id.get() << "\n";
    } else {
        std::cout << "Failed to connect to camera " << camera_id.get() << ": " << connect_result.error().message << "\n";
        return core::Err(connect_result.error());
    }
    
    return core::Ok();
}

core::VoidResult CLIInterface::cmd_disconnect_camera(const std::vector<std::string>& args) {
    core::CameraId camera_id{args[0]};
    
    std::cout << "Disconnecting camera " << camera_id.get() << "...\n";
    
    auto result = app_service_->disconnect_camera(camera_id);
    auto disconnect_result = result.get();
    
    if (disconnect_result.has_value()) {
        std::cout << "Successfully disconnected camera " << camera_id.get() << "\n";
    } else {
        std::cout << "Failed to disconnect camera " << camera_id.get() << ": " << disconnect_result.error().message << "\n";
        return core::Err(disconnect_result.error());
    }
    
    return core::Ok();
}

core::VoidResult CLIInterface::cmd_capture_image(const std::vector<std::string>& args) {
    core::CameraId camera_id{args[0]};
    
    std::cout << "Capturing image from camera " << camera_id.get() << "...\n";
    
    auto result = app_service_->capture_image(camera_id);
    auto image_result = result.get();
    
    if (image_result.has_value()) {
        const auto& image = image_result.value();
        std::cout << std::format("Successfully captured image: {}x{} pixels\n", image.width, image.height);
        
        // TODO: Save image to file if output_file is specified
        if (args.size() > 1) {
            std::cout << "Image saving not yet implemented\n";
        }
    } else {
        std::cout << "Failed to capture image: " << image_result.error().message << "\n";
        return core::Err(image_result.error());
    }
    
    return core::Ok();
}

core::VoidResult CLIInterface::cmd_system_status(const std::vector<std::string>& args) {
    auto result = app_service_->get_system_status();
    if (!result.has_value()) {
        std::cout << "Failed to get system status: " << result.error().message << "\n";
        return core::Err(result.error());
    }
    
    const auto& status = result.value();
    
    std::cout << "\nSystem Status:\n";
    std::cout << "==============\n";
    std::cout << std::format("  Running: {}\n", status.is_running ? "Yes" : "No");
    std::cout << std::format("  Uptime: {} seconds\n", 
        std::chrono::duration_cast<std::chrono::seconds>(status.uptime).count());
    std::cout << std::format("  Connected Cameras: {}\n", status.connected_cameras);
    std::cout << std::format("  Sapera Initialized: {}\n", status.sapera_initialized ? "Yes" : "No");
    std::cout << "\n";
    
    return core::Ok();
}

core::VoidResult CLIInterface::cmd_system_stats(const std::vector<std::string>& args) {
    auto result = app_service_->get_system_statistics();
    if (!result.has_value()) {
        std::cout << "Failed to get system statistics: " << result.error().message << "\n";
        return core::Err(result.error());
    }
    
    const auto& stats = result.value();
    
    std::cout << "\nSystem Statistics:\n";
    std::cout << "==================\n";
    std::cout << std::format("  Total Captures: {}\n", stats.total_captures);
    std::cout << std::format("  Successful Captures: {}\n", stats.successful_captures);
    std::cout << std::format("  Failed Captures: {}\n", stats.failed_captures);
    std::cout << std::format("  Success Rate: {:.1f}%\n", stats.success_rate * 100.0);
    std::cout << std::format("  Uptime: {} seconds\n", 
        std::chrono::duration_cast<std::chrono::seconds>(stats.uptime).count());
    std::cout << "\n";
    
    return core::Ok();
}

core::VoidResult CLIInterface::cmd_reset_system(const std::vector<std::string>& args) {
    std::cout << "Resetting system...\n";
    
    auto result = app_service_->reset_system();
    auto reset_result = result.get();
    
    if (reset_result.has_value()) {
        std::cout << "System reset successfully\n";
    } else {
        std::cout << "Failed to reset system: " << reset_result.error().message << "\n";
        return core::Err(reset_result.error());
    }
    
    return core::Ok();
}

core::VoidResult CLIInterface::cmd_help(const std::vector<std::string>& args) {
    if (args.empty()) {
        print_help();
    } else {
        print_command_help(args[0]);
    }
    return core::Ok();
}

core::VoidResult CLIInterface::cmd_exit(const std::vector<std::string>& args) {
    std::cout << "Exiting application...\n";
    stop_interactive_mode();
    return core::Ok();
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

utils::ApplicationConfig create_default_app_config() {
    return utils::ConfigurationManager::create_default_configuration();
}

void setup_signal_handlers(Application* app) {
    g_application = app;
    
#ifdef _WIN32
    // Windows signal handling
    signal(SIGINT, [](int signal) {
        if (g_application) {
            g_application->shutdown();
        }
        exit(0);
    });
    signal(SIGTERM, [](int signal) {
        if (g_application) {
            g_application->shutdown();
        }
        exit(0);
    });
#else
    // Unix signal handling
    signal(SIGINT, [](int signal) {
        if (g_application) {
            g_application->shutdown();
        }
        exit(0);
    });
    signal(SIGTERM, [](int signal) {
        if (g_application) {
            g_application->shutdown();
        }
        exit(0);
    });
#endif
}

void print_application_banner(std::shared_ptr<utils::Logger> logger) {
    logger->info("==========================================");
    logger->info("     SaperaCapture Pro v2.0.0");
    logger->info("     Modern Camera Control System");
    logger->info("==========================================");
}

void print_system_info(std::shared_ptr<utils::Logger> logger) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    logger->info("Platform: Windows");
    logger->info("Processors: {}", si.dwNumberOfProcessors);
#else
    struct utsname buffer;
    if (uname(&buffer) == 0) {
        logger->info("Platform: {} {}", buffer.sysname, buffer.release);
        logger->info("Architecture: {}", buffer.machine);
    }
#endif
}

core::VoidResult validate_environment() {
    // TODO: Implement environment validation
    // Check for Sapera SDK installation, required libraries, etc.
    return core::Ok();
}

} // namespace sapera::application 