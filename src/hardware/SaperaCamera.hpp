/**
 * SaperaCamera.hpp - Modern Sapera SDK Integration
 * Integrates the proven working Sapera SDK code with modern 2020+ architecture
 */

#pragma once

#include "../interfaces/CameraInterface.hpp"
#include "../reactive/EventSystem.hpp"
#include "../utils/Logger.hpp"
#include "../core/Types.hpp"
#include "../core/Result.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <optional>

// Forward declarations for Sapera SDK classes
class SapAcqDevice;
class SapBuffer;
class SapAcqDeviceToBuf;
class SapBufferWithTrash;

namespace sapera::hardware {

// =============================================================================
// SAPERA CAMERA INFORMATION
// =============================================================================

struct SaperaCameraInfo {
    core::CameraId camera_id;
    core::SerialNumber serial_number;
    core::ServerName server_name;
    uint32_t resource_index;
    std::string model_name;
    std::string display_name;
    bool is_connected = false;
    core::CameraStatus status = core::CameraStatus::Disconnected;
    
    [[nodiscard]] core::VoidResult validate() const;
};

// =============================================================================
// SAPERA DEVICE WRAPPER - RAII for Sapera objects
// =============================================================================

class SaperaDeviceWrapper {
private:
    std::unique_ptr<SapAcqDevice> acq_device_;
    std::unique_ptr<SapBufferWithTrash> buffer_;
    std::unique_ptr<SapAcqDeviceToBuf> transfer_;
    core::CameraId camera_id_;
    std::shared_ptr<utils::Logger> logger_;
    
public:
    explicit SaperaDeviceWrapper(const core::CameraId& camera_id);
    ~SaperaDeviceWrapper();
    
    // Non-copyable, movable
    SaperaDeviceWrapper(const SaperaDeviceWrapper&) = delete;
    SaperaDeviceWrapper& operator=(const SaperaDeviceWrapper&) = delete;
    SaperaDeviceWrapper(SaperaDeviceWrapper&&) = default;
    SaperaDeviceWrapper& operator=(SaperaDeviceWrapper&&) = default;
    
    /// Create and connect to Sapera device
    [[nodiscard]] core::VoidResult create_device(const core::ServerName& server_name, 
                                                 uint32_t resource_index);
    
    /// Destroy and disconnect from Sapera device
    void destroy_device();
    
    /// Check if device is properly created
    [[nodiscard]] bool is_created() const;
    
    /// Capture single image
    [[nodiscard]] core::Result<core::ImageData> capture_image();
    
    /// Get device feature value
    [[nodiscard]] core::Result<std::string> get_feature_value(const std::string& feature_name);
    
    /// Set device feature value
    [[nodiscard]] core::VoidResult set_feature_value(const std::string& feature_name, 
                                                     const std::string& value);
    
    /// Get image dimensions
    [[nodiscard]] core::Result<std::pair<uint32_t, uint32_t>> get_image_dimensions();
    
    /// Get raw Sapera objects (for advanced operations)
    [[nodiscard]] SapAcqDevice* get_acq_device() const { return acq_device_.get(); }
    [[nodiscard]] SapBufferWithTrash* get_buffer() const { return buffer_.get(); }
    [[nodiscard]] SapAcqDeviceToBuf* get_transfer() const { return transfer_.get(); }
    
private:
    [[nodiscard]] core::VoidResult setup_buffer();
    [[nodiscard]] core::VoidResult setup_transfer();
    void cleanup_resources();
};

// =============================================================================
// SAPERA CAMERA DISCOVERY SERVICE
// =============================================================================

class SaperaCameraDiscovery : public interfaces::ICameraDiscovery {
private:
    std::vector<SaperaCameraInfo> discovered_cameras_;
    std::shared_ptr<utils::Logger> logger_;
    mutable std::mutex discovery_mutex_;
    std::chrono::steady_clock::time_point last_discovery_time_;
    std::chrono::milliseconds discovery_cache_duration_{5000};
    
public:
    explicit SaperaCameraDiscovery(std::shared_ptr<utils::Logger> logger);
    
    // ICameraDiscovery interface
    [[nodiscard]] core::AsyncResult<std::vector<core::CameraInfo>> discover_cameras() override;
    [[nodiscard]] core::AsyncResult<core::CameraInfo> get_camera_info(const core::CameraId& camera_id) override;
    [[nodiscard]] core::AsyncResult<std::vector<core::CameraInfo>> refresh_camera_list() override;
    
    /// Get Sapera-specific camera info
    [[nodiscard]] core::Result<SaperaCameraInfo> get_sapera_camera_info(const core::CameraId& camera_id) const;
    
    /// Set discovery cache duration
    void set_cache_duration(std::chrono::milliseconds duration) { discovery_cache_duration_ = duration; }
    
private:
    [[nodiscard]] core::Result<std::vector<SaperaCameraInfo>> perform_discovery();
    [[nodiscard]] bool is_cache_valid() const;
    [[nodiscard]] core::CameraInfo convert_to_camera_info(const SaperaCameraInfo& sapera_info) const;
};

// =============================================================================
// SAPERA CAMERA IMPLEMENTATION
// =============================================================================

class SaperaCamera : public interfaces::ICamera {
private:
    core::CameraId camera_id_;
    SaperaCameraInfo camera_info_;
    std::unique_ptr<SaperaDeviceWrapper> device_wrapper_;
    std::shared_ptr<utils::Logger> logger_;
    std::shared_ptr<reactive::EventPublisher<reactive::CameraEvent>> event_publisher_;
    
    // Connection state
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> is_capturing_{false};
    std::atomic<core::CameraStatus> status_{core::CameraStatus::Disconnected};
    
    // Thread safety
    mutable std::mutex camera_mutex_;
    mutable std::mutex capture_mutex_;
    
    // Performance monitoring
    std::atomic<uint64_t> total_captures_{0};
    std::atomic<uint64_t> successful_captures_{0};
    std::atomic<uint64_t> failed_captures_{0};
    std::chrono::steady_clock::time_point last_capture_time_;
    
public:
    explicit SaperaCamera(const core::CameraId& camera_id, 
                         const SaperaCameraInfo& camera_info,
                         std::shared_ptr<utils::Logger> logger,
                         std::shared_ptr<reactive::EventPublisher<reactive::CameraEvent>> event_publisher);
    
    ~SaperaCamera() override;
    
    // Non-copyable, movable
    SaperaCamera(const SaperaCamera&) = delete;
    SaperaCamera& operator=(const SaperaCamera&) = delete;
    SaperaCamera(SaperaCamera&&) = default;
    SaperaCamera& operator=(SaperaCamera&&) = default;
    
    // ICameraConnection interface
    [[nodiscard]] core::AsyncResult<void> connect() override;
    [[nodiscard]] core::AsyncResult<void> disconnect() override;
    [[nodiscard]] bool is_connected() const override;
    [[nodiscard]] core::AsyncResult<void> reconnect() override;
    [[nodiscard]] core::AsyncResult<core::ConnectionDiagnostics> get_connection_diagnostics() override;
    
    // IImageCapture interface
    [[nodiscard]] core::AsyncResult<core::ImageData> capture_image() override;
    [[nodiscard]] core::AsyncResult<void> start_continuous_capture() override;
    [[nodiscard]] core::AsyncResult<void> stop_continuous_capture() override;
    [[nodiscard]] bool is_capturing() const override;
    [[nodiscard]] core::AsyncResult<void> capture_image_async(
        std::function<void(core::Result<core::ImageData>)> callback) override;
    
    // ICameraParameters interface
    [[nodiscard]] core::AsyncResult<core::ParameterValue> get_parameter(const core::ParameterName& name) override;
    [[nodiscard]] core::AsyncResult<void> set_parameter(const core::ParameterName& name, 
                                                        const core::ParameterValue& value) override;
    [[nodiscard]] core::AsyncResult<std::vector<core::ParameterInfo>> get_available_parameters() override;
    [[nodiscard]] core::AsyncResult<core::ParameterConstraints> get_parameter_constraints(
        const core::ParameterName& name) override;
    
    // ICameraMonitoring interface
    [[nodiscard]] core::AsyncResult<core::CameraStatus> get_status() override;
    [[nodiscard]] core::AsyncResult<core::CameraStatistics> get_statistics() override;
    [[nodiscard]] core::AsyncResult<core::CameraHealth> get_health() override;
    [[nodiscard]] core::AsyncResult<std::vector<core::CameraEvent>> get_recent_events() override;
    
    // ICamera interface
    [[nodiscard]] core::CameraId get_camera_id() const override;
    [[nodiscard]] core::AsyncResult<core::CameraInfo> get_camera_info() override;
    [[nodiscard]] core::AsyncResult<void> reset() override;
    
    /// Get Sapera-specific device wrapper (for advanced operations)
    [[nodiscard]] SaperaDeviceWrapper* get_device_wrapper() const { return device_wrapper_.get(); }
    
    /// Update camera info (for discovery refresh)
    void update_camera_info(const SaperaCameraInfo& new_info);
    
private:
    void publish_event(const reactive::CameraEvent& event);
    void update_statistics(bool capture_success, std::chrono::milliseconds duration);
    [[nodiscard]] core::Result<core::ParameterValue> convert_sapera_parameter(const std::string& sapera_value, 
                                                                              const core::ParameterName& param_name);
    [[nodiscard]] core::Result<std::string> convert_to_sapera_parameter(const core::ParameterValue& value);
    [[nodiscard]] core::VoidResult validate_parameter_value(const core::ParameterName& name, 
                                                           const core::ParameterValue& value);
};

// =============================================================================
// SAPERA CAMERA FACTORY
// =============================================================================

class SaperaCameraFactory : public interfaces::ICameraFactory {
private:
    std::shared_ptr<SaperaCameraDiscovery> discovery_service_;
    std::shared_ptr<utils::Logger> logger_;
    std::shared_ptr<reactive::EventPublisher<reactive::CameraEvent>> event_publisher_;
    
public:
    explicit SaperaCameraFactory(std::shared_ptr<utils::Logger> logger,
                                std::shared_ptr<reactive::EventPublisher<reactive::CameraEvent>> event_publisher);
    
    // ICameraFactory interface
    [[nodiscard]] core::AsyncResult<std::unique_ptr<interfaces::ICamera>> create_camera(
        const core::CameraId& camera_id) override;
    [[nodiscard]] core::AsyncResult<std::vector<core::CameraInfo>> discover_cameras() override;
    [[nodiscard]] core::AsyncResult<bool> is_camera_available(const core::CameraId& camera_id) override;
    
    /// Get discovery service
    [[nodiscard]] std::shared_ptr<SaperaCameraDiscovery> get_discovery_service() const { return discovery_service_; }
    
private:
    [[nodiscard]] core::Result<SaperaCameraInfo> get_camera_info_for_creation(const core::CameraId& camera_id);
};

// =============================================================================
// SAPERA SYSTEM MANAGER
// =============================================================================

class SaperaSystemManager {
private:
    std::shared_ptr<utils::Logger> logger_;
    std::shared_ptr<reactive::EventPublisher<reactive::CameraEvent>> event_publisher_;
    std::unique_ptr<SaperaCameraFactory> camera_factory_;
    std::map<core::CameraId, std::unique_ptr<SaperaCamera>> active_cameras_;
    std::mutex system_mutex_;
    
    // System state
    std::atomic<bool> is_initialized_{false};
    std::atomic<bool> is_shutting_down_{false};
    
public:
    explicit SaperaSystemManager(std::shared_ptr<utils::Logger> logger);
    ~SaperaSystemManager();
    
    /// Initialize Sapera system
    [[nodiscard]] core::VoidResult initialize();
    
    /// Shutdown Sapera system
    void shutdown();
    
    /// Get camera factory
    [[nodiscard]] SaperaCameraFactory* get_camera_factory() const { return camera_factory_.get(); }
    
    /// Get or create camera
    [[nodiscard]] core::Result<SaperaCamera*> get_camera(const core::CameraId& camera_id);
    
    /// Remove camera
    void remove_camera(const core::CameraId& camera_id);
    
    /// Get system statistics
    [[nodiscard]] core::Result<std::map<std::string, std::string>> get_system_statistics() const;
    
    /// Check if system is initialized
    [[nodiscard]] bool is_initialized() const { return is_initialized_; }
    
private:
    void cleanup_resources();
};

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/// Initialize Sapera SDK globally
[[nodiscard]] core::VoidResult initialize_sapera_sdk();

/// Shutdown Sapera SDK globally
void shutdown_sapera_sdk();

/// Convert Sapera error to Result error
[[nodiscard]] core::Error convert_sapera_error(const std::string& operation, const std::string& context = "");

/// Get Sapera SDK version information
[[nodiscard]] core::Result<std::string> get_sapera_version();

/// Validate camera ID format
[[nodiscard]] bool is_valid_camera_id(const core::CameraId& camera_id);

/// Convert raw image data to modern ImageData structure
[[nodiscard]] core::Result<core::ImageData> convert_sapera_image_data(void* sapera_data, 
                                                                     uint32_t width, 
                                                                     uint32_t height,
                                                                     uint32_t bytes_per_pixel);

} // namespace sapera::hardware 