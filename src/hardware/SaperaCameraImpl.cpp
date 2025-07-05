/**
 * SaperaCameraImpl.cpp - SaperaCamera Class Implementation
 * Implementation of the main SaperaCamera class and system management
 */

#include "SaperaCamera.hpp"
#include <future>
#include <chrono>
#include <algorithm>

namespace sapera::hardware {

// =============================================================================
// SAPERA CAMERA IMPLEMENTATION
// =============================================================================

SaperaCamera::SaperaCamera(const core::CameraId& camera_id, 
                          const SaperaCameraInfo& camera_info,
                          std::shared_ptr<utils::Logger> logger,
                          std::shared_ptr<reactive::EventPublisher<reactive::CameraEvent>> event_publisher)
    : camera_id_(camera_id)
    , camera_info_(camera_info)
    , logger_(logger)
    , event_publisher_(event_publisher)
    , last_capture_time_(std::chrono::steady_clock::now())
{
    device_wrapper_ = std::make_unique<SaperaDeviceWrapper>(camera_id);
    logger_->debug("Created SaperaCamera for {}", camera_id_.get());
}

SaperaCamera::~SaperaCamera() {
    if (is_connected()) {
        disconnect().wait();
    }
    logger_->debug("Destroyed SaperaCamera for {}", camera_id_.get());
}

// ICameraConnection interface
core::AsyncResult<void> SaperaCamera::connect() {
    return std::async(std::launch::async, [this]() -> core::VoidResult {
        std::lock_guard<std::mutex> lock(camera_mutex_);
        
        if (is_connected_.load()) {
            logger_->debug("Camera {} already connected", camera_id_.get());
            return core::Ok();
        }
        
        logger_->info("Connecting to camera {}", camera_id_.get());
        
        // Create Sapera device
        auto result = device_wrapper_->create_device(camera_info_.server_name, camera_info_.resource_index);
        if (!result.has_value()) {
            status_.store(core::CameraStatus::Error);
            publish_event(reactive::CameraErrorEvent{camera_id_, result.error()});
            return result;
        }
        
        // Update state
        is_connected_.store(true);
        status_.store(core::CameraStatus::Connected);
        
        // Publish connection event
        publish_event(reactive::CameraConnectedEvent{camera_id_, camera_info_.serial_number});
        
        logger_->info("Successfully connected to camera {}", camera_id_.get());
        return core::Ok();
    });
}

core::AsyncResult<void> SaperaCamera::disconnect() {
    return std::async(std::launch::async, [this]() -> core::VoidResult {
        std::lock_guard<std::mutex> lock(camera_mutex_);
        
        if (!is_connected_.load()) {
            logger_->debug("Camera {} already disconnected", camera_id_.get());
            return core::Ok();
        }
        
        logger_->info("Disconnecting camera {}", camera_id_.get());
        
        // Stop any ongoing captures
        if (is_capturing_.load()) {
            stop_continuous_capture().wait();
        }
        
        // Destroy Sapera device
        device_wrapper_->destroy_device();
        
        // Update state
        is_connected_.store(false);
        status_.store(core::CameraStatus::Disconnected);
        
        // Publish disconnection event
        publish_event(reactive::CameraDisconnectedEvent{camera_id_, "User requested"});
        
        logger_->info("Successfully disconnected camera {}", camera_id_.get());
        return core::Ok();
    });
}

bool SaperaCamera::is_connected() const {
    return is_connected_.load();
}

core::AsyncResult<void> SaperaCamera::reconnect() {
    return std::async(std::launch::async, [this]() -> core::VoidResult {
        logger_->info("Reconnecting camera {}", camera_id_.get());
        
        // Disconnect first
        auto disconnect_result = disconnect();
        auto disconnect_status = disconnect_result.wait_for(std::chrono::seconds(10));
        
        if (disconnect_status == std::future_status::timeout) {
            return core::Err(core::make_error(core::ErrorCode::CameraConnectionError,
                "Disconnect timeout during reconnection"));
        }
        
        if (!disconnect_result.get().has_value()) {
            return disconnect_result.get();
        }
        
        // Brief delay before reconnecting
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Reconnect
        auto connect_result = connect();
        auto connect_status = connect_result.wait_for(std::chrono::seconds(10));
        
        if (connect_status == std::future_status::timeout) {
            return core::Err(core::make_error(core::ErrorCode::CameraConnectionError,
                "Connect timeout during reconnection"));
        }
        
        return connect_result.get();
    });
}

core::AsyncResult<core::ConnectionDiagnostics> SaperaCamera::get_connection_diagnostics() {
    return std::async(std::launch::async, [this]() -> core::Result<core::ConnectionDiagnostics> {
        core::ConnectionDiagnostics diagnostics;
        diagnostics.camera_id = camera_id_;
        diagnostics.is_connected = is_connected_.load();
        diagnostics.connection_uptime = std::chrono::steady_clock::now() - last_capture_time_;
        diagnostics.last_error = core::Error{}; // TODO: Track last error
        
        if (is_connected()) {
            diagnostics.signal_strength = 100.0; // Placeholder
            diagnostics.packet_loss_rate = 0.0;  // Placeholder
        }
        
        return core::Ok(diagnostics);
    });
}

// IImageCapture interface
core::AsyncResult<core::ImageData> SaperaCamera::capture_image() {
    return std::async(std::launch::async, [this]() -> core::Result<core::ImageData> {
        std::lock_guard<std::mutex> lock(capture_mutex_);
        
        if (!is_connected_.load()) {
            return core::Err<core::ImageData>(core::make_error(core::ErrorCode::CameraNotConnected,
                "Camera not connected"));
        }
        
        auto start_time = std::chrono::steady_clock::now();
        total_captures_++;
        
        // Capture image through device wrapper
        auto result = device_wrapper_->capture_image();
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (result.has_value()) {
            successful_captures_++;
            last_capture_time_ = end_time;
            
            // Add metadata to image
            auto& image_data = result.value();
            image_data.camera_id = camera_id_;
            image_data.capture_id = core::CaptureId{std::to_string(total_captures_.load())};
            
            // Log successful capture
            logger_->log_image_captured(camera_id_, image_data.width, image_data.height, duration);
            
            // Publish capture event
            publish_event(reactive::ImageCapturedEvent{camera_id_, image_data.capture_id, image_data.timestamp});
        } else {
            failed_captures_++;
            logger_->log_camera_error(camera_id_, result.error());
            publish_event(reactive::CameraErrorEvent{camera_id_, result.error()});
        }
        
        update_statistics(result.has_value(), duration);
        return result;
    });
}

core::AsyncResult<void> SaperaCamera::start_continuous_capture() {
    return std::async(std::launch::async, [this]() -> core::VoidResult {
        std::lock_guard<std::mutex> lock(capture_mutex_);
        
        if (!is_connected_.load()) {
            return core::Err(core::make_error(core::ErrorCode::CameraNotConnected,
                "Camera not connected"));
        }
        
        if (is_capturing_.load()) {
            return core::Ok(); // Already capturing
        }
        
        logger_->info("Starting continuous capture for camera {}", camera_id_.get());
        
        // TODO: Implement continuous capture logic
        // For now, just mark as capturing
        is_capturing_.store(true);
        status_.store(core::CameraStatus::Capturing);
        
        return core::Ok();
    });
}

core::AsyncResult<void> SaperaCamera::stop_continuous_capture() {
    return std::async(std::launch::async, [this]() -> core::VoidResult {
        std::lock_guard<std::mutex> lock(capture_mutex_);
        
        if (!is_capturing_.load()) {
            return core::Ok(); // Not capturing
        }
        
        logger_->info("Stopping continuous capture for camera {}", camera_id_.get());
        
        // TODO: Implement continuous capture stop logic
        is_capturing_.store(false);
        status_.store(core::CameraStatus::Connected);
        
        return core::Ok();
    });
}

bool SaperaCamera::is_capturing() const {
    return is_capturing_.load();
}

core::AsyncResult<void> SaperaCamera::capture_image_async(
    std::function<void(core::Result<core::ImageData>)> callback) {
    return std::async(std::launch::async, [this, callback]() -> core::VoidResult {
        auto result = capture_image();
        callback(result.get());
        return core::Ok();
    });
}

// ICameraParameters interface
core::AsyncResult<core::ParameterValue> SaperaCamera::get_parameter(const core::ParameterName& name) {
    return std::async(std::launch::async, [this, name]() -> core::Result<core::ParameterValue> {
        if (!is_connected_.load()) {
            return core::Err<core::ParameterValue>(core::make_error(core::ErrorCode::CameraNotConnected,
                "Camera not connected"));
        }
        
        // Get parameter from Sapera device
        auto result = device_wrapper_->get_feature_value(name.get());
        if (!result.has_value()) {
            return core::Err<core::ParameterValue>(result.error());
        }
        
        // Convert Sapera parameter to modern parameter value
        return convert_sapera_parameter(result.value(), name);
    });
}

core::AsyncResult<void> SaperaCamera::set_parameter(const core::ParameterName& name, 
                                                   const core::ParameterValue& value) {
    return std::async(std::launch::async, [this, name, value]() -> core::VoidResult {
        if (!is_connected_.load()) {
            return core::Err(core::make_error(core::ErrorCode::CameraNotConnected,
                "Camera not connected"));
        }
        
        // Validate parameter value
        if (auto validation_result = validate_parameter_value(name, value); !validation_result.has_value()) {
            return validation_result;
        }
        
        // Convert modern parameter to Sapera format
        auto sapera_value_result = convert_to_sapera_parameter(value);
        if (!sapera_value_result.has_value()) {
            return core::Err(sapera_value_result.error());
        }
        
        // Set parameter on Sapera device
        return device_wrapper_->set_feature_value(name.get(), sapera_value_result.value());
    });
}

core::AsyncResult<std::vector<core::ParameterInfo>> SaperaCamera::get_available_parameters() {
    return std::async(std::launch::async, [this]() -> core::Result<std::vector<core::ParameterInfo>> {
        if (!is_connected_.load()) {
            return core::Err<std::vector<core::ParameterInfo>>(core::make_error(core::ErrorCode::CameraNotConnected,
                "Camera not connected"));
        }
        
        // TODO: Implement parameter enumeration
        // For now, return basic parameters
        std::vector<core::ParameterInfo> parameters;
        
        // Add basic parameters
        parameters.push_back(core::ParameterInfo{
            core::ParameterName{"ExposureTime"},
            core::ParameterType::Double,
            "Camera exposure time in microseconds",
            true, // readable
            true, // writable
            {}    // constraints (to be filled)
        });
        
        parameters.push_back(core::ParameterInfo{
            core::ParameterName{"Gain"},
            core::ParameterType::Double,
            "Camera gain value",
            true, // readable
            true, // writable
            {}    // constraints (to be filled)
        });
        
        return core::Ok(parameters);
    });
}

core::AsyncResult<core::ParameterConstraints> SaperaCamera::get_parameter_constraints(
    const core::ParameterName& name) {
    return std::async(std::launch::async, [this, name]() -> core::Result<core::ParameterConstraints> {
        if (!is_connected_.load()) {
            return core::Err<core::ParameterConstraints>(core::make_error(core::ErrorCode::CameraNotConnected,
                "Camera not connected"));
        }
        
        // TODO: Implement parameter constraints retrieval
        // For now, return basic constraints
        core::ParameterConstraints constraints;
        constraints.parameter_name = name;
        constraints.min_value = 0.0;
        constraints.max_value = 1000.0;
        constraints.step_size = 1.0;
        constraints.allowed_values = {};
        
        return core::Ok(constraints);
    });
}

// ICameraMonitoring interface
core::AsyncResult<core::CameraStatus> SaperaCamera::get_status() {
    return std::async(std::launch::async, [this]() -> core::Result<core::CameraStatus> {
        return core::Ok(status_.load());
    });
}

core::AsyncResult<core::CameraStatistics> SaperaCamera::get_statistics() {
    return std::async(std::launch::async, [this]() -> core::Result<core::CameraStatistics> {
        core::CameraStatistics stats;
        stats.camera_id = camera_id_;
        stats.total_captures = total_captures_.load();
        stats.successful_captures = successful_captures_.load();
        stats.failed_captures = failed_captures_.load();
        stats.uptime = std::chrono::steady_clock::now() - last_capture_time_;
        
        if (stats.total_captures > 0) {
            stats.success_rate = static_cast<double>(stats.successful_captures) / stats.total_captures;
        } else {
            stats.success_rate = 0.0;
        }
        
        return core::Ok(stats);
    });
}

core::AsyncResult<core::CameraHealth> SaperaCamera::get_health() {
    return std::async(std::launch::async, [this]() -> core::Result<core::CameraHealth> {
        core::CameraHealth health;
        health.camera_id = camera_id_;
        health.is_healthy = is_connected_.load() && status_.load() != core::CameraStatus::Error;
        health.temperature = 25.0; // Placeholder
        health.last_check = std::chrono::steady_clock::now();
        
        return core::Ok(health);
    });
}

core::AsyncResult<std::vector<core::CameraEvent>> SaperaCamera::get_recent_events() {
    return std::async(std::launch::async, [this]() -> core::Result<std::vector<core::CameraEvent>> {
        // TODO: Implement event history
        std::vector<core::CameraEvent> events;
        return core::Ok(events);
    });
}

// ICamera interface
core::CameraId SaperaCamera::get_camera_id() const {
    return camera_id_;
}

core::AsyncResult<core::CameraInfo> SaperaCamera::get_camera_info() {
    return std::async(std::launch::async, [this]() -> core::Result<core::CameraInfo> {
        core::CameraInfo info;
        info.camera_id = camera_info_.camera_id;
        info.serial_number = camera_info_.serial_number;
        info.model_name = camera_info_.model_name;
        info.display_name = camera_info_.display_name;
        info.is_connected = is_connected_.load();
        info.status = status_.load();
        info.camera_type = core::CameraType::Industrial;
        info.vendor_name = "Teledyne DALSA";
        info.interface_type = "GigE Vision";
        
        return core::Ok(info);
    });
}

core::AsyncResult<void> SaperaCamera::reset() {
    return std::async(std::launch::async, [this]() -> core::VoidResult {
        logger_->info("Resetting camera {}", camera_id_.get());
        
        // Perform reconnection to reset the camera
        return reconnect().get();
    });
}

// Private methods
void SaperaCamera::publish_event(const reactive::CameraEvent& event) {
    if (event_publisher_) {
        event_publisher_->publish(event);
    }
}

void SaperaCamera::update_statistics(bool capture_success, std::chrono::milliseconds duration) {
    // Statistics are updated atomically in the capture methods
    // This method can be used for additional processing if needed
}

core::Result<core::ParameterValue> SaperaCamera::convert_sapera_parameter(const std::string& sapera_value, 
                                                                         const core::ParameterName& param_name) {
    // TODO: Implement proper parameter conversion based on parameter type
    // For now, return as string
    return core::Ok(core::ParameterValue{sapera_value});
}

core::Result<std::string> SaperaCamera::convert_to_sapera_parameter(const core::ParameterValue& value) {
    // TODO: Implement proper parameter conversion
    // For now, use string representation
    return value.to_string();
}

core::VoidResult SaperaCamera::validate_parameter_value(const core::ParameterName& name, 
                                                       const core::ParameterValue& value) {
    // TODO: Implement parameter validation
    // For now, just check if value is valid
    if (value.to_string().value_or("").empty()) {
        return core::Err(core::make_error(core::ErrorCode::InvalidParameterValue,
            "Parameter value cannot be empty"));
    }
    
    return core::Ok();
}

void SaperaCamera::update_camera_info(const SaperaCameraInfo& new_info) {
    std::lock_guard<std::mutex> lock(camera_mutex_);
    camera_info_ = new_info;
}

// =============================================================================
// SAPERA CAMERA FACTORY IMPLEMENTATION
// =============================================================================

SaperaCameraFactory::SaperaCameraFactory(std::shared_ptr<utils::Logger> logger,
                                        std::shared_ptr<reactive::EventPublisher<reactive::CameraEvent>> event_publisher)
    : logger_(logger)
    , event_publisher_(event_publisher)
{
    discovery_service_ = std::make_shared<SaperaCameraDiscovery>(logger);
    logger_->debug("Created SaperaCameraFactory");
}

core::AsyncResult<std::unique_ptr<interfaces::ICamera>> SaperaCameraFactory::create_camera(
    const core::CameraId& camera_id) {
    return std::async(std::launch::async, [this, camera_id]() -> core::Result<std::unique_ptr<interfaces::ICamera>> {
        logger_->info("Creating camera {}", camera_id.get());
        
        // Get camera info for creation
        auto camera_info_result = get_camera_info_for_creation(camera_id);
        if (!camera_info_result.has_value()) {
            return core::Err<std::unique_ptr<interfaces::ICamera>>(camera_info_result.error());
        }
        
        // Create SaperaCamera
        auto camera = std::make_unique<SaperaCamera>(camera_id, camera_info_result.value(), 
                                                    logger_, event_publisher_);
        
        logger_->info("Successfully created camera {}", camera_id.get());
        return core::Ok<std::unique_ptr<interfaces::ICamera>>(std::move(camera));
    });
}

core::AsyncResult<std::vector<core::CameraInfo>> SaperaCameraFactory::discover_cameras() {
    return discovery_service_->discover_cameras();
}

core::AsyncResult<bool> SaperaCameraFactory::is_camera_available(const core::CameraId& camera_id) {
    return std::async(std::launch::async, [this, camera_id]() -> core::Result<bool> {
        auto camera_info_result = discovery_service_->get_camera_info(camera_id);
        auto status = camera_info_result.wait_for(std::chrono::seconds(5));
        
        if (status == std::future_status::timeout) {
            return core::Ok(false);
        }
        
        return core::Ok(camera_info_result.get().has_value());
    });
}

core::Result<SaperaCameraInfo> SaperaCameraFactory::get_camera_info_for_creation(const core::CameraId& camera_id) {
    return discovery_service_->get_sapera_camera_info(camera_id);
}

// =============================================================================
// SAPERA SYSTEM MANAGER IMPLEMENTATION
// =============================================================================

SaperaSystemManager::SaperaSystemManager(std::shared_ptr<utils::Logger> logger)
    : logger_(logger)
{
    event_publisher_ = std::make_shared<reactive::EventPublisher<reactive::CameraEvent>>();
    logger_->debug("Created SaperaSystemManager");
}

SaperaSystemManager::~SaperaSystemManager() {
    shutdown();
}

core::VoidResult SaperaSystemManager::initialize() {
    std::lock_guard<std::mutex> lock(system_mutex_);
    
    if (is_initialized_.load()) {
        return core::Ok();
    }
    
    logger_->info("Initializing Sapera System Manager");
    
    // Initialize Sapera SDK
    if (auto result = initialize_sapera_sdk(); !result.has_value()) {
        return result;
    }
    
    // Create camera factory
    camera_factory_ = std::make_unique<SaperaCameraFactory>(logger_, event_publisher_);
    
    is_initialized_.store(true);
    logger_->info("Sapera System Manager initialized successfully");
    
    return core::Ok();
}

void SaperaSystemManager::shutdown() {
    std::lock_guard<std::mutex> lock(system_mutex_);
    
    if (!is_initialized_.load() || is_shutting_down_.load()) {
        return;
    }
    
    logger_->info("Shutting down Sapera System Manager");
    is_shutting_down_.store(true);
    
    cleanup_resources();
    
    is_initialized_.store(false);
    is_shutting_down_.store(false);
    
    logger_->info("Sapera System Manager shut down successfully");
}

core::Result<SaperaCamera*> SaperaSystemManager::get_camera(const core::CameraId& camera_id) {
    std::lock_guard<std::mutex> lock(system_mutex_);
    
    if (!is_initialized_.load()) {
        return core::Err<SaperaCamera*>(core::make_error(core::ErrorCode::CameraInitializationError,
            "System not initialized"));
    }
    
    // Check if camera already exists
    auto it = active_cameras_.find(camera_id);
    if (it != active_cameras_.end()) {
        return core::Ok(it->second.get());
    }
    
    // Create new camera
    auto camera_result = camera_factory_->create_camera(camera_id);
    auto status = camera_result.wait_for(std::chrono::seconds(10));
    
    if (status == std::future_status::timeout) {
        return core::Err<SaperaCamera*>(core::make_error(core::ErrorCode::CameraConnectionError,
            "Camera creation timeout"));
    }
    
    auto camera_interface = camera_result.get();
    if (!camera_interface.has_value()) {
        return core::Err<SaperaCamera*>(camera_interface.error());
    }
    
    // Cast to SaperaCamera
    auto sapera_camera = dynamic_cast<SaperaCamera*>(camera_interface.value().get());
    if (!sapera_camera) {
        return core::Err<SaperaCamera*>(core::make_error(core::ErrorCode::CameraError,
            "Failed to cast to SaperaCamera"));
    }
    
    // Transfer ownership and store
    auto camera_ptr = static_cast<SaperaCamera*>(camera_interface.value().release());
    active_cameras_[camera_id] = std::unique_ptr<SaperaCamera>(camera_ptr);
    
    return core::Ok(camera_ptr);
}

void SaperaSystemManager::remove_camera(const core::CameraId& camera_id) {
    std::lock_guard<std::mutex> lock(system_mutex_);
    
    auto it = active_cameras_.find(camera_id);
    if (it != active_cameras_.end()) {
        logger_->info("Removing camera {}", camera_id.get());
        active_cameras_.erase(it);
    }
}

core::Result<std::map<std::string, std::string>> SaperaSystemManager::get_system_statistics() const {
    std::lock_guard<std::mutex> lock(system_mutex_);
    
    std::map<std::string, std::string> stats;
    stats["initialized"] = is_initialized_.load() ? "true" : "false";
    stats["active_cameras"] = std::to_string(active_cameras_.size());
    stats["shutting_down"] = is_shutting_down_.load() ? "true" : "false";
    
    return core::Ok(stats);
}

void SaperaSystemManager::cleanup_resources() {
    // Disconnect all cameras
    for (auto& [camera_id, camera] : active_cameras_) {
        if (camera && camera->is_connected()) {
            camera->disconnect().wait();
        }
    }
    
    // Clear cameras
    active_cameras_.clear();
    
    // Reset factory
    camera_factory_.reset();
    
    // Shutdown Sapera SDK
    shutdown_sapera_sdk();
}

} // namespace sapera::hardware 