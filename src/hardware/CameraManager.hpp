#pragma once

#include <vector>
#include <map>
#include <string>
#include <memory>
#include <functional>
#include <thread>
#include "CameraTypes.hpp"
#include "SapClassBasic.h"

// Forward declare settings structs from global namespace
struct CameraOrderSettings;

namespace SaperaCapturePro {

class CameraManager {
 public:
  static CameraManager& GetInstance();
  
  // Delete copy and move
  CameraManager(const CameraManager&) = delete;
  CameraManager& operator=(const CameraManager&) = delete;
  
  // Camera discovery and connection
  void DiscoverCameras(std::function<void(const std::string&)> log_callback = nullptr);
  void ConnectAllCameras(std::function<void(const std::string&)> log_callback = nullptr);
  void DisconnectAllCameras();
  
  // Capture parameters structure (thread-safe, pass by value)
  struct CaptureParams {
    int parallel_groups = 1;      // Number of cameras to capture simultaneously
    int group_delay_ms = 750;     // Delay between groups
    int stagger_delay_ms = 150;   // Delay between cameras within group
  };

  // Camera capture
  bool CaptureAllCameras(const std::string& session_path, const CaptureParams& params);
  void CaptureAllCamerasAsync(const std::string& session_path, const CaptureParams& params, std::function<void(const std::string&)> log_callback = nullptr);
  bool CaptureCamera(const std::string& camera_id, const std::string& session_path);
  
  // Camera parameter control
  void ApplyParameterToAllCameras(const std::string& feature_name, const std::string& value);
  void ApplyParameterToCamera(const std::string& camera_id, const std::string& feature_name, const std::string& value);
  void DetectCameraResolution();

  // Safe parameter application with type checking
  bool ApplySafeParameter(SapAcqDevice* device, const std::string& cameraId, const std::string& featureName, const std::string& value);

  // Helper method to save image from buffer with color conversion
  bool SaveImageFromBuffer(SapBuffer* buffer, const std::string& fullPath, const std::string& cameraName);
  
  // Color conversion configuration
  struct ColorConfig {
    int color_method = 1;              // 1-7
    int bayer_align = 2;               // 0..5 mapping to SapColorConversion::Align*
    bool use_hardware = false;
    std::string color_output_format = "RGB888"; // RGB888, RGB8888, RGB101010
    float gamma = 1.0f;
    float wb_gain_r = 1.0f;
    float wb_gain_g = 1.0f;
    float wb_gain_b = 1.0f;
    float wb_offset_r = 0.0f;
    float wb_offset_g = 0.0f;
    float wb_offset_b = 0.0f;
  };
  void SetColorConfig(const ColorConfig& cfg) { color_config_ = cfg; }

  // Getters
  const std::vector<CameraInfo>& GetDiscoveredCameras() const { return discovered_cameras_; }
  std::vector<CameraInfo> GetOrderedCameras() const;  // Returns cameras sorted by display position
  bool IsConnected(const std::string& camera_id) const;
  int GetConnectedCount() const { return static_cast<int>(connected_devices_.size()); }
  bool IsDiscovering() const { return is_discovering_; }
  bool IsConnecting() const { return is_connecting_; }
  bool IsCapturing() const { return is_capturing_; }

  // Camera ordering
  void ApplyCameraOrdering(const ::CameraOrderSettings& order_settings);
  std::string GetOrderedCameraName(const std::string& serial_number, int fallback_index) const;
  
  // Settings
  void SetExposureTime(int exposure_us) { exposure_time_ = exposure_us; }
  int GetExposureTime() const { return exposure_time_; }
  void SetCaptureFormat(bool raw) { capture_format_raw_ = raw; }
  bool GetCaptureFormat() const { return capture_format_raw_; }
  
  // Camera parameters
  struct Parameters {
    int width = 2560;
    int height = 1600;
    int max_width = 2560;
    int max_height = 1600;
    std::string pixel_format = "RGB8";
    int exposure_time = 40000;
    float gain = 0.0f;
    float wb_red = 1.0f;
    float wb_green = 1.0f;
    float wb_blue = 1.0f;
    bool auto_white_balance = false;
    int packet_size = 1200;
    int packet_delay = 3000;
  };
  
  Parameters& GetParameters() { return params_; }
  const Parameters& GetParameters() const { return params_; }

 private:
  CameraManager();
  ~CameraManager();
  
  std::vector<CameraInfo> discovered_cameras_;
  std::map<std::string, SapAcqDevice*> connected_devices_;
  std::map<std::string, SapBuffer*> connected_buffers_;
  std::map<std::string, SapAcqDeviceToBuf*> connected_transfers_;
  
  bool is_discovering_ = false;
  bool is_connecting_ = false;
  bool is_capturing_ = false;
  std::thread discovery_thread_;
  std::thread connection_thread_;
  std::thread capture_thread_;
  
  int exposure_time_ = 40000;
  bool capture_format_raw_ = false;
  Parameters params_;
  ColorConfig color_config_;
  
  std::function<void(const std::string&)> log_callback_;
  
  void Log(const std::string& message);
};

}  // namespace SaperaCapturePro