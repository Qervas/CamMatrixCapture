/**
 * Types.hpp - Modern Type-Safe Core Types (C++23)
 * Strong typing, std::expected error handling, and clean interfaces
 */

#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <chrono>
#include <memory>
#include <vector>
#include <functional>
#include <concepts>
#include <ranges>

namespace sapera::core {

// =============================================================================
// STRONG TYPES - No more string-based everything!
// =============================================================================

template<typename T, typename Tag>
class StrongType {
public:
    explicit StrongType(T value) : value_(std::move(value)) {}
    
    [[nodiscard]] const T& get() const { return value_; }
    [[nodiscard]] T& get() { return value_; }
    
    // Comparison operators
    auto operator<=>(const StrongType&) const = default;
    bool operator==(const StrongType&) const = default;
    
private:
    T value_;
};

// Camera ID - no more string IDs that can be mixed up!
struct CameraIdTag {};
using CameraId = StrongType<std::string, CameraIdTag>;

// Server name - type-safe server identification
struct ServerNameTag {};
using ServerName = StrongType<std::string, ServerNameTag>;

// Serial number - unique camera identification
struct SerialNumberTag {};
using SerialNumber = StrongType<std::string, SerialNumberTag>;

// File path - type-safe file handling
struct FilePathTag {};
using FilePath = StrongType<std::string, FilePathTag>;

// =============================================================================
// MODERN ERROR HANDLING - std::expected instead of boolean hell
// =============================================================================

// Error types - comprehensive and structured
enum class ErrorCode {
    // System errors
    NotInitialized,
    AlreadyInitialized,
    InvalidConfiguration,
    
    // Camera errors
    CameraNotFound,
    CameraAlreadyConnected,
    CameraNotConnected,
    CameraConnectionFailed,
    CameraDisconnectionFailed,
    
    // Capture errors
    CaptureNotReady,
    CaptureTimeout,
    CaptureBufferFull,
    CaptureFormatUnsupported,
    
    // Hardware errors
    HardwareError,
    SaperaSDKError,
    DriverError,
    
    // I/O errors
    FileNotFound,
    FileWriteError,
    FileReadError,
    DirectoryNotFound,
    
    // Parameter errors
    ParameterNotFound,
    ParameterReadOnly,
    ParameterOutOfRange,
    ParameterInvalidType,
    
    // Network errors
    NetworkError,
    ServerNotFound,
    ConnectionTimeout,
    
    // Generic errors
    UnknownError,
    OperationCancelled,
    InsufficientMemory,
    InvalidArgument
};

// Rich error information
struct Error {
    ErrorCode code;
    std::string message;
    std::string details;
    std::chrono::system_clock::time_point timestamp;
    std::optional<std::string> context;
    
    Error(ErrorCode code, std::string_view message, std::string_view details = "")
        : code(code)
        , message(message)
        , details(details)
        , timestamp(std::chrono::system_clock::now())
    {}
    
    Error with_context(std::string_view ctx) && {
        context = ctx;
        return *this;
    }
};

// Modern Result type - no more boolean returns!
template<typename T>
using Result = std::expected<T, Error>;

// Helper for void results
using VoidResult = Result<void>;

// =============================================================================
// CAMERA TYPES - Modern enum classes and structures
// =============================================================================

enum class CameraState {
    Disconnected,
    Connecting,
    Connected,
    Ready,
    Capturing,
    Error,
    Maintenance
};

enum class CameraType {
    Industrial,
    LineScanner,
    AreaScanner,
    Thermal,
    Multispectral,
    Unknown
};

// Image format - type-safe image handling
enum class ImageFormat {
    Mono8,
    Mono16,
    RGB8,
    RGB16,
    YUV422,
    TIFF,
    Unknown
};

// =============================================================================
// CAMERA INFORMATION - Clean, modern structures
// =============================================================================

struct CameraCapabilities {
    std::vector<ImageFormat> supportedFormats;
    std::pair<uint32_t, uint32_t> maxResolution;
    std::pair<uint32_t, uint32_t> minResolution;
    std::pair<double, double> frameRateRange;
    std::vector<std::string> supportedFeatures;
    bool supportsTriggering;
    bool supportsMultipleBuffers;
    bool supportsHardwareTrigger;
};

struct CameraInfo {
    CameraId id;
    ServerName serverName;
    SerialNumber serialNumber;
    std::string modelName;
    std::string displayName;
    CameraType type;
    CameraState state;
    CameraCapabilities capabilities;
    int32_t resourceIndex;
    std::chrono::system_clock::time_point lastSeen;
    std::optional<std::string> firmwareVersion;
    std::optional<std::string> driverVersion;
    
    [[nodiscard]] bool isConnected() const {
        return state == CameraState::Connected || 
               state == CameraState::Ready || 
               state == CameraState::Capturing;
    }
    
    [[nodiscard]] bool isReady() const {
        return state == CameraState::Ready;
    }
};

// =============================================================================
// IMAGE HANDLING - Modern, type-safe image structures
// =============================================================================

struct ImageMetadata {
    std::chrono::system_clock::time_point timestamp;
    SerialNumber cameraSerial;
    uint64_t frameNumber;
    ImageFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t bitsPerPixel;
    uint32_t bytesPerPixel;
    size_t dataSize;
    std::optional<double> exposureTime;
    std::optional<double> gain;
    std::optional<std::string> triggerMode;
};

// Modern image buffer with RAII
class ImageBuffer {
private:
    std::unique_ptr<uint8_t[]> data_;
    ImageMetadata metadata_;
    
public:
    ImageBuffer(size_t size, ImageMetadata metadata)
        : data_(std::make_unique<uint8_t[]>(size))
        , metadata_(std::move(metadata)) 
    {}
    
    // Move-only semantics
    ImageBuffer(const ImageBuffer&) = delete;
    ImageBuffer& operator=(const ImageBuffer&) = delete;
    ImageBuffer(ImageBuffer&&) = default;
    ImageBuffer& operator=(ImageBuffer&&) = default;
    
    [[nodiscard]] const uint8_t* data() const { return data_.get(); }
    [[nodiscard]] uint8_t* data() { return data_.get(); }
    [[nodiscard]] const ImageMetadata& metadata() const { return metadata_; }
    [[nodiscard]] size_t size() const { return metadata_.dataSize; }
    
    // Modern save functionality
    [[nodiscard]] VoidResult save_to_file(const FilePath& path) const;
};

// =============================================================================
// CAPTURE SETTINGS - Type-safe configuration
// =============================================================================

struct CaptureSettings {
    ImageFormat format = ImageFormat::Mono8;
    std::optional<std::pair<uint32_t, uint32_t>> resolution;
    std::optional<double> frameRate;
    std::optional<double> exposureTime;
    std::optional<double> gain;
    std::optional<std::string> triggerMode;
    uint32_t bufferCount = 3;
    bool enableTimestamp = true;
    bool enableMetadata = true;
    std::chrono::milliseconds timeout{5000};
    
    [[nodiscard]] VoidResult validate() const;
};

// =============================================================================
// STATISTICS AND MONITORING - Modern metrics
// =============================================================================

struct CaptureStatistics {
    uint64_t framesReceived = 0;
    uint64_t framesDropped = 0;
    uint64_t bytesReceived = 0;
    double averageFrameRate = 0.0;
    double currentFrameRate = 0.0;
    std::chrono::system_clock::time_point lastFrameTime;
    std::chrono::milliseconds averageLatency{0};
    uint32_t bufferUtilization = 0;
    
    [[nodiscard]] double dropRate() const {
        if (framesReceived == 0) return 0.0;
        return static_cast<double>(framesDropped) / static_cast<double>(framesReceived + framesDropped);
    }
};

// =============================================================================
// CONCEPTS - Modern C++ type constraints
// =============================================================================

template<typename T>
concept CameraObserver = requires(T t, const CameraInfo& info) {
    { t.on_camera_connected(info) } -> std::same_as<void>;
    { t.on_camera_disconnected(info) } -> std::same_as<void>;
    { t.on_camera_error(info, Error{}) } -> std::same_as<void>;
};

template<typename T>
concept ImageObserver = requires(T t, const ImageBuffer& buffer) {
    { t.on_image_captured(buffer) } -> std::same_as<void>;
    { t.on_capture_error(Error{}) } -> std::same_as<void>;
};

// =============================================================================
// MODERN CALLBACK SYSTEM - std::function with type safety
// =============================================================================

using CameraEventCallback = std::function<void(const CameraInfo&)>;
using ImageCaptureCallback = std::function<void(const ImageBuffer&)>;
using ErrorCallback = std::function<void(const Error&)>;

} // namespace sapera::core 