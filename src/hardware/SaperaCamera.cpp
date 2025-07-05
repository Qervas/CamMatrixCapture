/**
 * SaperaCamera.cpp - Modern Sapera SDK Integration Implementation
 * Integrates proven working Sapera SDK code with modern architecture
 */

#include "SaperaCamera.hpp"
#include <future>
#include <chrono>
#include <algorithm>
#include <iostream>

// Include Sapera SDK headers
#include "sapclassbasic.h"

namespace sapera::hardware {

// =============================================================================
// SAPERA CAMERA INFO IMPLEMENTATION
// =============================================================================

core::VoidResult SaperaCameraInfo::validate() const {
    if (camera_id.get().empty()) {
        return core::Err(core::make_error(core::ErrorCode::InvalidCameraId,
            "Camera ID cannot be empty"));
    }
    
    if (server_name.get().empty()) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
            "Server name cannot be empty"));
    }
    
    if (serial_number.get().empty()) {
        return core::Err(core::make_error(core::ErrorCode::InvalidConfiguration,
            "Serial number cannot be empty"));
    }
    
    return core::Ok();
}

// =============================================================================
// SAPERA DEVICE WRAPPER IMPLEMENTATION
// =============================================================================

SaperaDeviceWrapper::SaperaDeviceWrapper(const core::CameraId& camera_id)
    : camera_id_(camera_id)
    , logger_(utils::get_camera_logger(camera_id))
{
    logger_->debug("Created SaperaDeviceWrapper for camera {}", camera_id_.get());
}

SaperaDeviceWrapper::~SaperaDeviceWrapper() {
    destroy_device();
}

core::VoidResult SaperaDeviceWrapper::create_device(const core::ServerName& server_name, 
                                                   uint32_t resource_index) {
    logger_->info("Creating Sapera device for camera {} on server {} (resource {})", 
        camera_id_.get(), server_name.get(), resource_index);
    
    try {
        // Create acquisition device
        acq_device_ = std::make_unique<SapAcqDevice>(server_name.get().c_str(), resource_index);
        if (!acq_device_->Create()) {
            cleanup_resources();
            return core::Err(convert_sapera_error("create_acquisition_device", 
                "Failed to create SapAcqDevice"));
        }
        
        // Setup buffer
        if (auto result = setup_buffer(); !result.has_value()) {
            cleanup_resources();
            return result;
        }
        
        // Setup transfer
        if (auto result = setup_transfer(); !result.has_value()) {
            cleanup_resources();
            return result;
        }
        
        logger_->info("Successfully created Sapera device for camera {}", camera_id_.get());
        return core::Ok();
        
    } catch (const std::exception& e) {
        cleanup_resources();
        return core::Err(core::make_error(core::ErrorCode::CameraConnectionError,
            "Exception creating Sapera device: " + std::string(e.what())));
    }
}

void SaperaDeviceWrapper::destroy_device() {
    if (acq_device_ || buffer_ || transfer_) {
        logger_->info("Destroying Sapera device for camera {}", camera_id_.get());
        cleanup_resources();
    }
}

bool SaperaDeviceWrapper::is_created() const {
    return acq_device_ && buffer_ && transfer_ && 
           acq_device_->IsCreated() && buffer_->IsCreated() && transfer_->IsCreated();
}

core::Result<core::ImageData> SaperaDeviceWrapper::capture_image() {
    if (!is_created()) {
        return core::Err<core::ImageData>(core::make_error(core::ErrorCode::CameraNotConnected,
            "Device not properly created"));
    }
    
    logger_->debug("Capturing image from camera {}", camera_id_.get());
    
    try {
        // Perform single frame capture
        if (!transfer_->Snap()) {
            return core::Err<core::ImageData>(convert_sapera_error("capture_snap", 
                "Failed to snap image"));
        }
        
        // Get image dimensions
        auto dimensions_result = get_image_dimensions();
        if (!dimensions_result.has_value()) {
            return core::Err<core::ImageData>(dimensions_result.error());
        }
        
        auto [width, height] = dimensions_result.value();
        
        // Get buffer data
        void* buffer_data = buffer_->GetAddress();
        if (!buffer_data) {
            return core::Err<core::ImageData>(core::make_error(core::ErrorCode::CaptureError,
                "Failed to get buffer address"));
        }
        
        // Determine bytes per pixel (simplified - assume 8-bit grayscale for now)
        uint32_t bytes_per_pixel = 1;
        
        // Convert to modern ImageData structure
        return convert_sapera_image_data(buffer_data, width, height, bytes_per_pixel);
        
    } catch (const std::exception& e) {
        return core::Err<core::ImageData>(core::make_error(core::ErrorCode::CaptureError,
            "Exception during image capture: " + std::string(e.what())));
    }
}

core::Result<std::string> SaperaDeviceWrapper::get_feature_value(const std::string& feature_name) {
    if (!is_created()) {
        return core::Err<std::string>(core::make_error(core::ErrorCode::CameraNotConnected,
            "Device not properly created"));
    }
    
    try {
        char buffer[512];
        if (acq_device_->GetFeatureValue(feature_name.c_str(), buffer, sizeof(buffer))) {
            return core::Ok(std::string(buffer));
        } else {
            return core::Err<std::string>(convert_sapera_error("get_feature_value", 
                "Failed to get feature: " + feature_name));
        }
    } catch (const std::exception& e) {
        return core::Err<std::string>(core::make_error(core::ErrorCode::ParameterError,
            "Exception getting feature: " + std::string(e.what())));
    }
}

core::VoidResult SaperaDeviceWrapper::set_feature_value(const std::string& feature_name, 
                                                       const std::string& value) {
    if (!is_created()) {
        return core::Err(core::make_error(core::ErrorCode::CameraNotConnected,
            "Device not properly created"));
    }
    
    try {
        if (acq_device_->SetFeatureValue(feature_name.c_str(), value.c_str())) {
            return core::Ok();
        } else {
            return core::Err(convert_sapera_error("set_feature_value", 
                "Failed to set feature: " + feature_name + " = " + value));
        }
    } catch (const std::exception& e) {
        return core::Err(core::make_error(core::ErrorCode::ParameterError,
            "Exception setting feature: " + std::string(e.what())));
    }
}

core::Result<std::pair<uint32_t, uint32_t>> SaperaDeviceWrapper::get_image_dimensions() {
    if (!is_created()) {
        return core::Err<std::pair<uint32_t, uint32_t>>(core::make_error(core::ErrorCode::CameraNotConnected,
            "Device not properly created"));
    }
    
    try {
        int width = buffer_->GetWidth();
        int height = buffer_->GetHeight();
        
        if (width <= 0 || height <= 0) {
            return core::Err<std::pair<uint32_t, uint32_t>>(core::make_error(core::ErrorCode::CaptureError,
                "Invalid image dimensions"));
        }
        
        return core::Ok(std::make_pair(static_cast<uint32_t>(width), static_cast<uint32_t>(height)));
        
    } catch (const std::exception& e) {
        return core::Err<std::pair<uint32_t, uint32_t>>(core::make_error(core::ErrorCode::CaptureError,
            "Exception getting image dimensions: " + std::string(e.what())));
    }
}

core::VoidResult SaperaDeviceWrapper::setup_buffer() {
    try {
        // Create buffer for image capture
        buffer_ = std::make_unique<SapBufferWithTrash>(1, acq_device_.get());
        if (!buffer_->Create()) {
            return core::Err(convert_sapera_error("create_buffer", 
                "Failed to create SapBufferWithTrash"));
        }
        
        return core::Ok();
        
    } catch (const std::exception& e) {
        return core::Err(core::make_error(core::ErrorCode::CameraConnectionError,
            "Exception creating buffer: " + std::string(e.what())));
    }
}

core::VoidResult SaperaDeviceWrapper::setup_transfer() {
    try {
        // Create transfer object
        transfer_ = std::make_unique<SapAcqDeviceToBuf>(acq_device_.get(), buffer_.get());
        if (!transfer_->Create()) {
            return core::Err(convert_sapera_error("create_transfer", 
                "Failed to create SapAcqDeviceToBuf"));
        }
        
        return core::Ok();
        
    } catch (const std::exception& e) {
        return core::Err(core::make_error(core::ErrorCode::CameraConnectionError,
            "Exception creating transfer: " + std::string(e.what())));
    }
}

void SaperaDeviceWrapper::cleanup_resources() {
    if (transfer_) {
        transfer_->Destroy();
        transfer_.reset();
    }
    
    if (buffer_) {
        buffer_->Destroy();
        buffer_.reset();
    }
    
    if (acq_device_) {
        acq_device_->Destroy();
        acq_device_.reset();
    }
}

// =============================================================================
// SAPERA CAMERA DISCOVERY IMPLEMENTATION
// =============================================================================

SaperaCameraDiscovery::SaperaCameraDiscovery(std::shared_ptr<utils::Logger> logger)
    : logger_(logger)
{
    logger_->debug("Created SaperaCameraDiscovery");
}

core::AsyncResult<std::vector<core::CameraInfo>> SaperaCameraDiscovery::discover_cameras() {
    return std::async(std::launch::async, [this]() -> core::Result<std::vector<core::CameraInfo>> {
        std::lock_guard<std::mutex> lock(discovery_mutex_);
        
        // Check if we can use cached results
        if (is_cache_valid()) {
            logger_->debug("Using cached camera discovery results");
            std::vector<core::CameraInfo> result;
            result.reserve(discovered_cameras_.size());
            for (const auto& sapera_info : discovered_cameras_) {
                result.push_back(convert_to_camera_info(sapera_info));
            }
            return core::Ok(result);
        }
        
        // Perform fresh discovery
        auto discovery_result = perform_discovery();
        if (!discovery_result.has_value()) {
            return core::Err<std::vector<core::CameraInfo>>(discovery_result.error());
        }
        
        discovered_cameras_ = discovery_result.value();
        last_discovery_time_ = std::chrono::steady_clock::now();
        
        // Convert to generic camera info
        std::vector<core::CameraInfo> result;
        result.reserve(discovered_cameras_.size());
        for (const auto& sapera_info : discovered_cameras_) {
            result.push_back(convert_to_camera_info(sapera_info));
        }
        
        logger_->info("Discovered {} cameras", result.size());
        return core::Ok(result);
    });
}

core::AsyncResult<core::CameraInfo> SaperaCameraDiscovery::get_camera_info(const core::CameraId& camera_id) {
    return std::async(std::launch::async, [this, camera_id]() -> core::Result<core::CameraInfo> {
        std::lock_guard<std::mutex> lock(discovery_mutex_);
        
        for (const auto& sapera_info : discovered_cameras_) {
            if (sapera_info.camera_id == camera_id) {
                return core::Ok(convert_to_camera_info(sapera_info));
            }
        }
        
        return core::Err<core::CameraInfo>(core::make_error(core::ErrorCode::CameraNotFound,
            "Camera not found: " + camera_id.get()));
    });
}

core::AsyncResult<std::vector<core::CameraInfo>> SaperaCameraDiscovery::refresh_camera_list() {
    return std::async(std::launch::async, [this]() -> core::Result<std::vector<core::CameraInfo>> {
        std::lock_guard<std::mutex> lock(discovery_mutex_);
        
        // Force fresh discovery by clearing cache
        discovered_cameras_.clear();
        
        auto discovery_result = perform_discovery();
        if (!discovery_result.has_value()) {
            return core::Err<std::vector<core::CameraInfo>>(discovery_result.error());
        }
        
        discovered_cameras_ = discovery_result.value();
        last_discovery_time_ = std::chrono::steady_clock::now();
        
        std::vector<core::CameraInfo> result;
        result.reserve(discovered_cameras_.size());
        for (const auto& sapera_info : discovered_cameras_) {
            result.push_back(convert_to_camera_info(sapera_info));
        }
        
        logger_->info("Refreshed camera list: {} cameras", result.size());
        return core::Ok(result);
    });
}

core::Result<SaperaCameraInfo> SaperaCameraDiscovery::get_sapera_camera_info(const core::CameraId& camera_id) const {
    std::lock_guard<std::mutex> lock(discovery_mutex_);
    
    for (const auto& sapera_info : discovered_cameras_) {
        if (sapera_info.camera_id == camera_id) {
            return core::Ok(sapera_info);
        }
    }
    
    return core::Err<SaperaCameraInfo>(core::make_error(core::ErrorCode::CameraNotFound,
        "Camera not found: " + camera_id.get()));
}

core::Result<std::vector<SaperaCameraInfo>> SaperaCameraDiscovery::perform_discovery() {
    logger_->info("Performing Sapera camera discovery...");
    
    std::vector<SaperaCameraInfo> cameras;
    
    try {
        // Get server count
        int server_count = SapManager::GetServerCount();
        logger_->debug("Found {} Sapera servers", server_count);
        
        if (server_count == 0) {
            logger_->warning("No Sapera servers found");
            return core::Ok(cameras);
        }
        
        // Enumerate all servers
        for (int server_index = 0; server_index < server_count; server_index++) {
            char server_name[256];
            if (!SapManager::GetServerName(server_index, server_name, sizeof(server_name))) {
                logger_->warning("Failed to get server name for index {}", server_index);
                continue;
            }
            
            logger_->debug("Processing server {}: {}", server_index, server_name);
            
            // Get acquisition device count for this server
            int resource_count = SapManager::GetResourceCount(server_name, SapManager::ResourceAcqDevice);
            logger_->debug("Server {} has {} acquisition devices", server_name, resource_count);
            
            // Enumerate acquisition devices
            for (int resource_index = 0; resource_index < resource_count; resource_index++) {
                try {
                    // Create acquisition device temporarily for discovery
                    SapAcqDevice acq_device(server_name, resource_index);
                    if (!acq_device.Create()) {
                        logger_->warning("Failed to create discovery device for server {} resource {}", 
                            server_name, resource_index);
                        continue;
                    }
                    
                    // Extract camera information
                    SaperaCameraInfo camera_info;
                    camera_info.camera_id = core::CameraId{std::to_string(cameras.size() + 1)};
                    camera_info.server_name = core::ServerName{server_name};
                    camera_info.resource_index = resource_index;
                    
                    // Get serial number
                    char buffer[512];
                    if (acq_device.GetFeatureValue("DeviceSerialNumber", buffer, sizeof(buffer))) {
                        camera_info.serial_number = core::SerialNumber{std::string(buffer)};
                    } else {
                        camera_info.serial_number = core::SerialNumber{"Unknown_" + std::to_string(resource_index)};
                    }
                    
                    // Get model name
                    if (acq_device.GetFeatureValue("DeviceModelName", buffer, sizeof(buffer))) {
                        camera_info.model_name = std::string(buffer);
                    } else {
                        camera_info.model_name = "Unknown_Model";
                    }
                    
                    camera_info.display_name = camera_info.model_name + "_" + camera_info.camera_id.get();
                    camera_info.is_connected = false;
                    camera_info.status = core::CameraStatus::Disconnected;
                    
                    // Validate camera info
                    if (auto validation_result = camera_info.validate(); !validation_result.has_value()) {
                        logger_->warning("Invalid camera info for server {} resource {}: {}", 
                            server_name, resource_index, validation_result.error().message);
                        continue;
                    }
                    
                    cameras.push_back(camera_info);
                    
                    logger_->info("Discovered camera: {} ({})", 
                        camera_info.serial_number.get(), camera_info.model_name);
                    
                    // Cleanup discovery device
                    acq_device.Destroy();
                    
                } catch (const std::exception& e) {
                    logger_->warning("Exception during camera discovery for server {} resource {}: {}", 
                        server_name, resource_index, e.what());
                }
            }
        }
        
        logger_->info("Camera discovery completed: {} cameras found", cameras.size());
        return core::Ok(cameras);
        
    } catch (const std::exception& e) {
        return core::Err<std::vector<SaperaCameraInfo>>(core::make_error(core::ErrorCode::CameraDiscoveryError,
            "Exception during camera discovery: " + std::string(e.what())));
    }
}

bool SaperaCameraDiscovery::is_cache_valid() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_discovery_time_);
    return elapsed < discovery_cache_duration_ && !discovered_cameras_.empty();
}

core::CameraInfo SaperaCameraDiscovery::convert_to_camera_info(const SaperaCameraInfo& sapera_info) const {
    core::CameraInfo info;
    info.camera_id = sapera_info.camera_id;
    info.serial_number = sapera_info.serial_number;
    info.model_name = sapera_info.model_name;
    info.display_name = sapera_info.display_name;
    info.is_connected = sapera_info.is_connected;
    info.status = sapera_info.status;
    info.camera_type = core::CameraType::Industrial;
    info.vendor_name = "Teledyne DALSA";
    info.interface_type = "GigE Vision";
    
    return info;
}

// =============================================================================
// UTILITY FUNCTIONS IMPLEMENTATION
// =============================================================================

core::VoidResult initialize_sapera_sdk() {
    // Sapera SDK initialization is typically automatic
    // Just verify that the system is working
    int server_count = SapManager::GetServerCount();
    if (server_count < 0) {
        return core::Err(core::make_error(core::ErrorCode::CameraInitializationError,
            "Failed to initialize Sapera SDK"));
    }
    
    return core::Ok();
}

void shutdown_sapera_sdk() {
    // Sapera SDK shutdown is typically automatic
    // No explicit shutdown needed
}

core::Error convert_sapera_error(const std::string& operation, const std::string& context) {
    return core::make_error(core::ErrorCode::CameraError, 
        "Sapera operation failed: " + operation + 
        (context.empty() ? "" : " - " + context));
}

core::Result<std::string> get_sapera_version() {
    // This would require specific Sapera SDK version API
    // For now, return a placeholder
    return core::Ok(std::string("Sapera SDK 8.x"));
}

bool is_valid_camera_id(const core::CameraId& camera_id) {
    return !camera_id.get().empty() && camera_id.get().size() <= 256;
}

core::Result<core::ImageData> convert_sapera_image_data(void* sapera_data, 
                                                       uint32_t width, 
                                                       uint32_t height,
                                                       uint32_t bytes_per_pixel) {
    if (!sapera_data) {
        return core::Err<core::ImageData>(core::make_error(core::ErrorCode::CaptureError,
            "Null Sapera image data"));
    }
    
    if (width == 0 || height == 0) {
        return core::Err<core::ImageData>(core::make_error(core::ErrorCode::CaptureError,
            "Invalid image dimensions"));
    }
    
    try {
        core::ImageData image_data;
        image_data.width = width;
        image_data.height = height;
        image_data.bytes_per_pixel = bytes_per_pixel;
        image_data.pixel_format = core::PixelFormat::Mono8; // Simplified for now
        image_data.timestamp = std::chrono::steady_clock::now();
        
        // Calculate data size
        size_t data_size = width * height * bytes_per_pixel;
        
        // Copy image data
        image_data.data.resize(data_size);
        std::memcpy(image_data.data.data(), sapera_data, data_size);
        
        return core::Ok(image_data);
        
    } catch (const std::exception& e) {
        return core::Err<core::ImageData>(core::make_error(core::ErrorCode::CaptureError,
            "Exception converting Sapera image data: " + std::string(e.what())));
    }
}

} // namespace sapera::hardware 