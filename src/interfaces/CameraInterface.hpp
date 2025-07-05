/**
 * CameraInterface.hpp - Modern Camera Interface (Clean Architecture)
 * Dependency injection, concepts, and clean boundaries
 */

#pragma once

#include "../core/Types.hpp"
#include "../core/Result.hpp"
#include <memory>
#include <span>
#include <ranges>

namespace sapera::interfaces {

// =============================================================================
// CAMERA DISCOVERY INTERFACE - Clean, testable camera discovery
// =============================================================================

class ICameraDiscovery {
public:
    virtual ~ICameraDiscovery() = default;
    
    /// Discover all available cameras
    [[nodiscard]] virtual core::Result<std::vector<core::CameraInfo>> discover_cameras() = 0;
    
    /// Refresh camera list (useful for hot-plugging)
    [[nodiscard]] virtual core::VoidResult refresh() = 0;
    
    /// Get camera by ID
    [[nodiscard]] virtual core::Result<core::CameraInfo> get_camera_info(const core::CameraId& id) = 0;
    
    /// Check if camera is available
    [[nodiscard]] virtual bool is_available(const core::CameraId& id) = 0;
};

// =============================================================================
// CAMERA CONNECTION INTERFACE - Type-safe connection management
// =============================================================================

class ICameraConnection {
public:
    virtual ~ICameraConnection() = default;
    
    /// Connect to camera
    [[nodiscard]] virtual core::VoidResult connect(const core::CameraId& id) = 0;
    
    /// Disconnect from camera
    [[nodiscard]] virtual core::VoidResult disconnect(const core::CameraId& id) = 0;
    
    /// Check if camera is connected
    [[nodiscard]] virtual bool is_connected(const core::CameraId& id) = 0;
    
    /// Get all connected cameras
    [[nodiscard]] virtual std::vector<core::CameraId> get_connected_cameras() = 0;
    
    /// Get connection health
    [[nodiscard]] virtual core::Result<core::CameraState> get_connection_state(const core::CameraId& id) = 0;
};

// =============================================================================
// IMAGE CAPTURE INTERFACE - Modern capture with async support
// =============================================================================

class IImageCapture {
public:
    virtual ~IImageCapture() = default;
    
    /// Capture single image
    [[nodiscard]] virtual core::Result<core::ImageBuffer> capture_image(
        const core::CameraId& id,
        const core::CaptureSettings& settings = {}
    ) = 0;
    
    /// Start continuous capture
    [[nodiscard]] virtual core::VoidResult start_continuous_capture(
        const core::CameraId& id,
        const core::CaptureSettings& settings,
        core::ImageCaptureCallback callback
    ) = 0;
    
    /// Stop continuous capture
    [[nodiscard]] virtual core::VoidResult stop_continuous_capture(const core::CameraId& id) = 0;
    
    /// Check if capturing
    [[nodiscard]] virtual bool is_capturing(const core::CameraId& id) = 0;
    
    /// Get capture statistics
    [[nodiscard]] virtual core::Result<core::CaptureStatistics> get_statistics(const core::CameraId& id) = 0;
};

// =============================================================================
// CAMERA PARAMETER INTERFACE - Type-safe parameter management
// =============================================================================

enum class ParameterType {
    Integer,
    Float,
    String,
    Boolean,
    Enumeration
};

struct ParameterInfo {
    std::string name;
    ParameterType type;
    std::string description;
    std::optional<std::string> unit;
    bool isReadOnly;
    bool isRequired;
    
    // Type-specific constraints
    std::optional<int64_t> minInt;
    std::optional<int64_t> maxInt;
    std::optional<double> minFloat;
    std::optional<double> maxFloat;
    std::vector<std::string> enumValues;
};

class ICameraParameters {
public:
    virtual ~ICameraParameters() = default;
    
    /// Get available parameters
    [[nodiscard]] virtual core::Result<std::vector<ParameterInfo>> get_available_parameters(
        const core::CameraId& id
    ) = 0;
    
    /// Get parameter value as string
    [[nodiscard]] virtual core::Result<std::string> get_parameter_string(
        const core::CameraId& id,
        std::string_view parameter_name
    ) = 0;
    
    /// Set parameter value from string
    [[nodiscard]] virtual core::VoidResult set_parameter_string(
        const core::CameraId& id,
        std::string_view parameter_name,
        std::string_view value
    ) = 0;
    
    /// Type-safe parameter getters
    [[nodiscard]] virtual core::Result<int64_t> get_parameter_int(
        const core::CameraId& id,
        std::string_view parameter_name
    ) = 0;
    
    [[nodiscard]] virtual core::Result<double> get_parameter_float(
        const core::CameraId& id,
        std::string_view parameter_name
    ) = 0;
    
    [[nodiscard]] virtual core::Result<bool> get_parameter_bool(
        const core::CameraId& id,
        std::string_view parameter_name
    ) = 0;
    
    /// Type-safe parameter setters
    [[nodiscard]] virtual core::VoidResult set_parameter_int(
        const core::CameraId& id,
        std::string_view parameter_name,
        int64_t value
    ) = 0;
    
    [[nodiscard]] virtual core::VoidResult set_parameter_float(
        const core::CameraId& id,
        std::string_view parameter_name,
        double value
    ) = 0;
    
    [[nodiscard]] virtual core::VoidResult set_parameter_bool(
        const core::CameraId& id,
        std::string_view parameter_name,
        bool value
    ) = 0;
};

// =============================================================================
// CAMERA MONITORING INTERFACE - Real-time monitoring
// =============================================================================

class ICameraMonitoring {
public:
    virtual ~ICameraMonitoring() = default;
    
    /// Get real-time camera health
    [[nodiscard]] virtual core::Result<core::CaptureStatistics> get_health(const core::CameraId& id) = 0;
    
    /// Subscribe to camera events
    [[nodiscard]] virtual core::VoidResult subscribe_to_events(
        const core::CameraId& id,
        core::CameraEventCallback callback
    ) = 0;
    
    /// Unsubscribe from events
    [[nodiscard]] virtual core::VoidResult unsubscribe_from_events(const core::CameraId& id) = 0;
    
    /// Get system-wide statistics
    [[nodiscard]] virtual core::Result<std::vector<core::CaptureStatistics>> get_system_statistics() = 0;
};

// =============================================================================
// UNIFIED CAMERA INTERFACE - Composition of all interfaces
// =============================================================================

class ICamera : public ICameraDiscovery, 
                public ICameraConnection, 
                public IImageCapture, 
                public ICameraParameters, 
                public ICameraMonitoring {
public:
    virtual ~ICamera() = default;
    
    /// Initialize the camera system
    [[nodiscard]] virtual core::VoidResult initialize() = 0;
    
    /// Shutdown the camera system
    [[nodiscard]] virtual core::VoidResult shutdown() = 0;
    
    /// Check if system is initialized
    [[nodiscard]] virtual bool is_initialized() const = 0;
    
    /// Get system information
    [[nodiscard]] virtual core::Result<std::string> get_system_info() = 0;
};

// =============================================================================
// CAMERA FACTORY INTERFACE - Modern factory pattern
// =============================================================================

class ICameraFactory {
public:
    virtual ~ICameraFactory() = default;
    
    /// Create camera instance
    [[nodiscard]] virtual std::unique_ptr<ICamera> create_camera() = 0;
    
    /// Get supported camera types
    [[nodiscard]] virtual std::vector<core::CameraType> get_supported_types() = 0;
    
    /// Check if factory can create cameras
    [[nodiscard]] virtual bool can_create_cameras() = 0;
    
    /// Get factory information
    [[nodiscard]] virtual std::string get_factory_info() = 0;
};

// =============================================================================
// CAMERA CONFIGURATION INTERFACE - Type-safe configuration
// =============================================================================

struct CameraConfiguration {
    std::optional<core::CaptureSettings> defaultCaptureSettings;
    std::optional<std::chrono::milliseconds> connectionTimeout;
    std::optional<std::chrono::milliseconds> captureTimeout;
    std::optional<uint32_t> maxBufferCount;
    std::optional<bool> enableAutoReconnect;
    std::optional<std::string> logLevel;
    std::map<std::string, std::string> customParameters;
    
    [[nodiscard]] core::VoidResult validate() const;
};

class ICameraConfiguration {
public:
    virtual ~ICameraConfiguration() = default;
    
    /// Load configuration from file
    [[nodiscard]] virtual core::Result<CameraConfiguration> load_from_file(const core::FilePath& path) = 0;
    
    /// Save configuration to file
    [[nodiscard]] virtual core::VoidResult save_to_file(
        const CameraConfiguration& config,
        const core::FilePath& path
    ) = 0;
    
    /// Get default configuration
    [[nodiscard]] virtual CameraConfiguration get_default_configuration() = 0;
    
    /// Validate configuration
    [[nodiscard]] virtual core::VoidResult validate_configuration(const CameraConfiguration& config) = 0;
};

// =============================================================================
// MODERN CONCEPTS - Type constraints for implementations
// =============================================================================

template<typename T>
concept CameraInterface = requires(T t) {
    requires std::derived_from<T, ICamera>;
    { t.initialize() } -> std::same_as<core::VoidResult>;
    { t.shutdown() } -> std::same_as<core::VoidResult>;
    { t.is_initialized() } -> std::same_as<bool>;
};

template<typename T>
concept CameraFactory = requires(T t) {
    requires std::derived_from<T, ICameraFactory>;
    { t.create_camera() } -> std::same_as<std::unique_ptr<ICamera>>;
    { t.can_create_cameras() } -> std::same_as<bool>;
};

// =============================================================================
// DEPENDENCY INJECTION CONTAINER - Modern DI
// =============================================================================

class DIContainer {
private:
    std::map<std::string, std::function<std::unique_ptr<void>()>> factories_;
    std::map<std::string, std::unique_ptr<void>> singletons_;
    
public:
    /// Register factory for type
    template<typename Interface, typename Implementation>
    void register_factory() {
        static_assert(std::is_base_of_v<Interface, Implementation>);
        factories_[typeid(Interface).name()] = []() -> std::unique_ptr<void> {
            return std::make_unique<Implementation>();
        };
    }
    
    /// Register singleton for type
    template<typename Interface, typename Implementation>
    void register_singleton() {
        static_assert(std::is_base_of_v<Interface, Implementation>);
        singletons_[typeid(Interface).name()] = std::make_unique<Implementation>();
    }
    
    /// Resolve type
    template<typename T>
    [[nodiscard]] std::unique_ptr<T> resolve() {
        const std::string type_name = typeid(T).name();
        
        // Check singletons first
        if (auto it = singletons_.find(type_name); it != singletons_.end()) {
            return std::unique_ptr<T>(static_cast<T*>(it->second.release()));
        }
        
        // Check factories
        if (auto it = factories_.find(type_name); it != factories_.end()) {
            return std::unique_ptr<T>(static_cast<T*>(it->second().release()));
        }
        
        return nullptr;
    }
};

} // namespace sapera::interfaces 