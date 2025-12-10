#include "CameraManager.hpp"
#include "../utils/SettingsManager.hpp"
#include <chrono>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace SaperaCapturePro {

CameraManager& CameraManager::GetInstance() {
  static CameraManager instance;
  return instance;
}

CameraManager::CameraManager() = default;

CameraManager::~CameraManager() {
  DisconnectAllCameras();
  
  if (discovery_thread_.joinable()) {
    discovery_thread_.join();
  }
  if (connection_thread_.joinable()) {
    connection_thread_.join();
  }
  if (capture_thread_.joinable()) {
    capture_thread_.join();
  }
}

void CameraManager::DiscoverCameras(std::function<void(const std::string&)> log_callback) {
  if (is_discovering_) {
    if (log_callback) log_callback("[DISC] Camera discovery already in progress...");
    return;
  }
  
  log_callback_ = log_callback;
  is_discovering_ = true;
  Log("[DISC] Starting camera discovery...");
  
  // Start discovery in a separate thread
  discovery_thread_ = std::thread([this]() {
    std::vector<CameraInfo> temp_cameras;
    
    // Get server count
    int serverCount = SapManager::GetServerCount();
    
    if (serverCount == 0) {
      Log("[NET] No Sapera servers found");
      is_discovering_ = false;
      return;
    }
    
    int cameraIndex = 1;
    
    // Enumerate all servers
    for (int serverIndex = 0; serverIndex < serverCount; serverIndex++) {
      char serverName[256];
      if (!SapManager::GetServerName(serverIndex, serverName, sizeof(serverName))) {
        Log("[NET] Failed to get server name for server " + std::to_string(serverIndex));
        continue;
      }
      
      // Skip system server
      if (std::string(serverName) == "System") {
        continue;
      }
      
      Log("[NET] Server " + std::to_string(serverIndex) + ": " + std::string(serverName));
      
      // Get acquisition device count for this server
      int resourceCount = SapManager::GetResourceCount(serverName, SapManager::ResourceAcqDevice);
      Log("[NET] Acquisition devices: " + std::to_string(resourceCount));
      
      // Fast enumerate: Just get resource names without creating full devices
      // We'll get detailed info (serial, model) later during connection
      for (int resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++) {
        try {
          char resourceName[256];
          if (SapManager::GetResourceName(serverName, SapManager::ResourceAcqDevice, resourceIndex, resourceName, sizeof(resourceName))) {

            CameraInfo camera;
            camera.id = std::to_string(cameraIndex);
            camera.serverName = serverName;
            camera.resourceIndex = resourceIndex;
            camera.serialNumber = std::string(resourceName); // Use resource name as temp ID
            camera.modelName = "Nano-C4020"; // Default model (will update on connect)

            // Create camera name for neural rendering
            std::string indexStr = std::to_string(cameraIndex);
            if (indexStr.length() == 1) indexStr = "0" + indexStr;
            camera.name = "cam_" + indexStr;
            camera.isConnected = false;
            camera.status = CameraStatus::Disconnected;
            camera.type = CameraType::Industrial;

            temp_cameras.push_back(camera);
            Log("[OK] Found: " + camera.name + " at " + std::string(serverName) + "[" + std::to_string(resourceIndex) + "]");

            cameraIndex++;
          }

        } catch (const std::exception& e) {
          Log("[NET] Exception: " + std::string(e.what()));
        }
      }
    }
    
    // Update global variables in main thread
    discovered_cameras_ = temp_cameras;
    
    Log("[OK] Discovery complete: " + std::to_string(discovered_cameras_.size()) + " cameras found");
    is_discovering_ = false;
  });
  
  // Detach the thread so it can run independently
  discovery_thread_.detach();
}

void CameraManager::ConnectAllCameras(std::function<void(const std::string&)> log_callback) {
  if (is_connecting_) {
    if (log_callback) log_callback("[NET] Camera connection already in progress...");
    return;
  }
  
  if (discovered_cameras_.empty()) {
    if (log_callback) log_callback("[NET] No cameras discovered. Run camera discovery first.");
    return;
  }
  
  log_callback_ = log_callback;
  is_connecting_ = true;
  Log("[NET] Starting camera connection...");
  
  // Start connection in a separate thread
  connection_thread_ = std::thread([this]() {
    std::map<std::string, SapAcqDevice*> temp_connected_devices;
    std::map<std::string, SapBuffer*> temp_connected_buffers;
    std::map<std::string, SapAcqDeviceToBuf*> temp_connected_transfers;
    int successCount = 0;
    
    for (const auto& camera : discovered_cameras_) {
      std::string cameraId = camera.id;
      
      try {
        // Create acquisition device using serverName and resourceIndex
        SapAcqDevice* acqDevice = new SapAcqDevice(camera.serverName.c_str(), camera.resourceIndex);
        if (!acqDevice->Create()) {
          Log("[NET] Failed to create acquisition device for " + camera.name);
          delete acqDevice;
          continue;
        }

        // Now that device is connected, get real serial number and model
        char infoBuffer[512];
        if (acqDevice->GetFeatureValue("DeviceSerialNumber", infoBuffer, sizeof(infoBuffer))) {
          // Update the camera info with real serial number
          for (auto& cam : discovered_cameras_) {
            if (cam.id == camera.id) {
              cam.serialNumber = std::string(infoBuffer);
              break;
            }
          }
        }
        if (acqDevice->GetFeatureValue("DeviceModelName", infoBuffer, sizeof(infoBuffer))) {
          for (auto& cam : discovered_cameras_) {
            if (cam.id == camera.id) {
              cam.modelName = std::string(infoBuffer);
              break;
            }
          }
        }

        // Note: Camera settings will be applied AFTER all cameras are connected
        // This prevents applying settings to partially connected cameras

        // Create buffer for image capture
        SapBuffer* buffer = new SapBufferWithTrash(1, acqDevice);
        if (!buffer->Create()) {
          Log("[NET] Failed to create buffer for " + camera.name);
          acqDevice->Destroy();
          delete acqDevice;
          delete buffer;
          continue;
        }
        
        // Create transfer object
        SapAcqDeviceToBuf* transfer = new SapAcqDeviceToBuf(acqDevice, buffer);
        if (!transfer->Create()) {
          Log("[NET] Failed to create transfer for " + camera.name);
          buffer->Destroy();
          acqDevice->Destroy();
          delete transfer;
          delete buffer;
          delete acqDevice;
          continue;
        }
        
        // Store connected components
        temp_connected_devices[cameraId] = acqDevice;
        temp_connected_buffers[cameraId] = buffer;
        temp_connected_transfers[cameraId] = transfer;
        
        successCount++;
        Log("[OK] " + camera.name + " connected successfully");
        
      } catch (const std::exception& e) {
        Log("[NET] Exception connecting " + camera.name + ": " + std::string(e.what()));
      }
    }
    
    // Update global variables
    connected_devices_ = temp_connected_devices;
    connected_buffers_ = temp_connected_buffers;
    connected_transfers_ = temp_connected_transfers;
    
    // Update camera connection status in discovered_cameras_ list
    for (auto& camera : discovered_cameras_) {
      if (connected_devices_.find(camera.id) != connected_devices_.end()) {
        camera.isConnected = true;
        camera.status = CameraStatus::Connected;
      } else {
        camera.isConnected = false;
        camera.status = CameraStatus::Disconnected;
      }
    }
    
    Log("[OK] Connection summary: " + std::to_string(successCount) + "/" + std::to_string(discovered_cameras_.size()) + " cameras connected");
    
    if (successCount == discovered_cameras_.size() && successCount > 0) {
      Log("[OK] All cameras connected successfully!");
      DetectCameraResolution();
    } else if (successCount > 0) {
      Log("[WARN] Partial connection: " + std::to_string(successCount) + "/" + std::to_string(discovered_cameras_.size()) + " cameras connected");
      DetectCameraResolution();
    } else {
      Log("[ERR] No cameras could be connected");
    }
    
    is_connecting_ = false;
  });
  
  connection_thread_.detach();
}

void CameraManager::DisconnectAllCameras() {
  if (connected_devices_.empty()) {
    Log("[NET] No cameras connected to disconnect");
    return;
  }

  int device_count = static_cast<int>(connected_devices_.size());
  Log("[NET] Disconnecting " + std::to_string(device_count) + " cameras...");

  // IMPORTANT: Sapera SDK requires disconnection in reverse order of creation:
  // 1. First disconnect and destroy transfers
  // 2. Then destroy buffers
  // 3. Finally destroy acquisition devices
  // This prevents "CorXferDisconnect" errors

  int destroyed_count = 0;

  // Step 1: Disconnect and destroy all transfers FIRST
  Log("[NET] Step 1: Disconnecting transfers...");
  for (auto& [id, transfer] : connected_transfers_) {
    if (transfer) {
      try {
        // Stop any ongoing transfer
        if (transfer->IsGrabbing()) {
          transfer->Freeze();
          transfer->Wait(1000);
        }
        // Disconnect from hardware
        if (transfer->IsConnected()) {
          transfer->Disconnect();
        }
        // Destroy the transfer object
        transfer->Destroy();
        delete transfer;
        Log("[OK] Transfer disconnected: " + id);
      } catch (const std::exception& e) {
        Log("[ERROR] Failed to destroy transfer " + id + ": " + e.what());
      }
    }
  }
  connected_transfers_.clear();

  // Step 2: Destroy all buffers
  Log("[NET] Step 2: Destroying buffers...");
  for (auto& [id, buffer] : connected_buffers_) {
    if (buffer) {
      try {
        buffer->Destroy();
        delete buffer;
        Log("[OK] Buffer destroyed: " + id);
      } catch (const std::exception& e) {
        Log("[ERROR] Failed to destroy buffer " + id + ": " + e.what());
      }
    }
  }
  connected_buffers_.clear();

  // Step 3: Destroy all acquisition devices LAST
  Log("[NET] Step 3: Destroying devices...");
  for (auto& [id, device] : connected_devices_) {
    if (device) {
      try {
        device->Destroy();
        delete device;
        destroyed_count++;
        Log("[OK] Device disconnected: " + id);
      } catch (const std::exception& e) {
        Log("[ERROR] Failed to destroy device " + id + ": " + e.what());
      }
    }
  }
  connected_devices_.clear();

  // Update camera connection status in discovered_cameras_ list
  for (auto& camera : discovered_cameras_) {
    camera.isConnected = false;
    camera.status = CameraStatus::Disconnected;
  }

  Log("[OK] Successfully disconnected " + std::to_string(destroyed_count) + "/" + std::to_string(device_count) + " cameras");
}

bool CameraManager::CaptureAllCameras(const std::string& session_path, const CaptureParams& params) {
  if (connected_devices_.empty()) {
    Log("[NET] No cameras connected");
    return false;
  }

  // Copy params to local variables to avoid any thread safety issues
  // Validate and adjust parameters for safety
  int parallel_groups = params.parallel_groups;
  const int delay_ms = params.group_delay_ms;
  const int stagger_ms = params.stagger_delay_ms;

  if (parallel_groups < 1) {
    Log("[ERR] Invalid parallel_groups: " + std::to_string(parallel_groups) + " (must be >= 1)");
    return false;
  }
  if (delay_ms < 0) {
    Log("[ERR] Invalid delay_ms: " + std::to_string(delay_ms) + " (must be >= 0)");
    return false;
  }
  if (stagger_ms < 0) {
    Log("[ERR] Invalid stagger_ms: " + std::to_string(stagger_ms) + " (must be >= 0)");
    return false;
  }

  // BANDWIDTH SAFETY: Prevent crashes from network saturation
  // Each 4112x3008 camera = ~50MB over 1Gbps network
  // Overlapping transfers cause crashes in Sapera SDK

  // Rule 1: Minimum stagger delay with parallel groups
  if (parallel_groups > 1 && stagger_ms < 100) {
    Log("[WARN] ⚠️ UNSAFE: Stagger delay " + std::to_string(stagger_ms) + "ms too low for parallel capture!");
    Log("[WARN] ⚠️ With " + std::to_string(parallel_groups) + " groups, network will saturate and crash");
    Log("[WARN] ⚠️ FORCING sequential mode (groups=1) to prevent crash");
    parallel_groups = 1;
  }

  // Rule 2: Maximum parallel groups limit
  if (parallel_groups > 4) {
    Log("[WARN] ⚠️ UNSAFE: " + std::to_string(parallel_groups) + " parallel groups exceeds 1Gbps bandwidth!");
    Log("[WARN] ⚠️ LIMITING to 4 groups maximum to prevent crashes");
    parallel_groups = 4;
  }

  // Rule 3: Actually, for true safety, force sequential
  // The "parallel_groups" setting doesn't actually capture in parallel anymore
  // It's purely sequential with stagger delays between cameras
  if (parallel_groups != 1) {
    Log("[INFO] Note: 'Parallel Groups' is a legacy setting name");
    Log("[INFO] All cameras capture sequentially with stagger delays for bandwidth safety");
  }

  // Create session directory
  std::filesystem::create_directories(session_path);

  Log("[REC] 🎬 PARALLEL GROUP CAPTURE starting...");
  Log("[IMG] Session path: " + session_path);
  Log("[REC] 📊 Parallel groups: " + std::to_string(parallel_groups) + " cameras simultaneously");
  Log("[REC] ⏱ Group delay: " + std::to_string(delay_ms) + "ms");
  Log("[REC] 🔀 Stagger delay: " + std::to_string(stagger_ms) + "ms (prevents bandwidth spikes)");

  auto startTime = std::chrono::high_resolution_clock::now();

  bool allSuccess = true;
  int successCount = 0;
  int totalCameras = static_cast<int>(connected_devices_.size());

  // Get list of connected and enabled cameras in order
  std::vector<CameraInfo> cameras_to_capture;
  {
    std::lock_guard<std::mutex> lock(disabled_cameras_mutex_);
    int cameraIndex = 0;
    for (const auto& camera : discovered_cameras_) {
      if (connected_devices_.find(camera.id) != connected_devices_.end()) {
        // Only include if camera is enabled
        if (disabled_cameras_.find(cameraIndex) == disabled_cameras_.end()) {
          cameras_to_capture.push_back(camera);
        } else {
          Log("[REC] Skipping disabled camera " + std::to_string(cameraIndex) + " (" + camera.name + ")");
        }
      }
      cameraIndex++;
    }
  }

  Log("[REC] 📸 Capturing from " + std::to_string(cameras_to_capture.size()) + " cameras in groups of " + std::to_string(parallel_groups) + "...");

  // Capture in groups
  for (size_t groupStart = 0; groupStart < cameras_to_capture.size(); groupStart += parallel_groups) {
    size_t groupEnd = std::min(groupStart + parallel_groups, cameras_to_capture.size());
    size_t groupSize = groupEnd - groupStart;
    int groupNumber = static_cast<int>(groupStart / parallel_groups) + 1;
    int totalGroups = static_cast<int>((cameras_to_capture.size() + parallel_groups - 1) / parallel_groups);

    Log("[REC] 📦 Group " + std::to_string(groupNumber) + "/" + std::to_string(totalGroups) +
        " (" + std::to_string(groupSize) + " cameras)");

    // Sequential capture with proper bandwidth management
    // Trigger Snap(), Wait(), then add delay before next camera to avoid bandwidth saturation
    std::vector<bool> capture_complete(groupSize, false);

    // SAPERA-SAFE SEQUENTIAL CAPTURE
    // Use simple Snap+Wait pattern - Sapera SDK handles threading internally
    for (size_t i = 0; i < groupSize; ++i) {
      const auto& camera = cameras_to_capture[groupStart + i];

      // Thread-safe access to transfer object
      SapAcqDeviceToBuf* transfer = nullptr;
      {
        std::lock_guard<std::mutex> lock(camera_mutex_);
        auto transferIt = connected_transfers_.find(camera.id);
        if (transferIt == connected_transfers_.end()) {
          Log("[ERR] ❌ Transfer not found for " + camera.name);
          continue;
        }
        transfer = transferIt->second;
      }

      if (!transfer) {
        Log("[ERR] ❌ Null transfer for " + camera.name);
        continue;
      }

      try {
        Log("[REC] 📷 Capturing " + camera.name);

        // Simple Snap+Wait - no retries, no complex logic
        // Sapera SDK is designed to handle this reliably
        if (!transfer->Snap()) {
          Log("[ERR] ❌ Snap failed for " + camera.name);
          allSuccess = false;
          continue;
        }

        if (!transfer->Wait(5000)) {
          Log("[ERR] ❌ Wait timeout for " + camera.name);
          allSuccess = false;
          continue;
        }

        capture_complete[i] = true;
        Log("[REC] ✓ " + camera.name + " complete");

        // Stagger delay to prevent bandwidth spikes
        if (i < groupSize - 1 && stagger_ms > 0) {
          std::this_thread::sleep_for(std::chrono::milliseconds(stagger_ms));
        }

      } catch (...) {
        Log("[ERR] ❌ Exception capturing " + camera.name);
        allSuccess = false;
      }
    }

    int completeCount = 0;
    for (size_t i = 0; i < groupSize; ++i) {
      if (capture_complete[i]) completeCount++;
    }
    Log("[REC] ✅ " + std::to_string(completeCount) + "/" + std::to_string(groupSize) + " cameras completed");

    // Phase 3: Save all captured images
    for (size_t i = 0; i < groupSize; ++i) {
      if (!capture_complete[i]) continue;

      const auto& camera = cameras_to_capture[groupStart + i];

      // Thread-safe access to buffer
      SapBuffer* buffer = nullptr;
      {
        std::lock_guard<std::mutex> lock(camera_mutex_);
        auto bufferIt = connected_buffers_.find(camera.id);
        if (bufferIt == connected_buffers_.end()) {
          Log("[ERR] ❌ Buffer not found for " + camera.name);
          allSuccess = false;
          continue;
        }
        buffer = bufferIt->second;
      }

      if (!buffer) {
        Log("[ERR] ❌ Null buffer for " + camera.name);
        allSuccess = false;
        continue;
      }

      try {
        // Generate filename using camera name (consistent ordering)
        std::string extension = capture_format_raw_ ? ".raw" : ".tiff";
        std::string filename = camera.name + extension;
        std::string fullPath = session_path + "/" + filename;

        // Save the captured image
        if (SaveImageFromBuffer(buffer, fullPath, camera.name)) {
          Log("[OK] ✅ " + camera.name + " saved");
          successCount++;
        } else {
          Log("[ERR] ❌ Save failed: " + camera.name);
          allSuccess = false;
        }

      } catch (...) {
        Log("[ERR] ❌ Exception saving " + camera.name);
        allSuccess = false;
      }
    }

    Log("[REC] ✓ Group " + std::to_string(groupNumber) + " completed");

    // Add delay between groups (except for the last group)
    if (groupEnd < cameras_to_capture.size()) {
      Log("[REC] ⏳ Waiting " + std::to_string(delay_ms) + "ms before next group...");
      std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

  Log("[REC] 🏁 Capture completed in " + std::to_string(duration.count()) + "ms");
  Log("[REC] 📊 Success rate: " + std::to_string(successCount) + "/" + std::to_string(totalCameras) + " cameras");

  return allSuccess;
}

void CameraManager::CaptureAllCamerasAsync(const std::string& session_path, const CaptureParams& params, std::function<void(const std::string&)> log_callback) {
  if (is_capturing_) {
    if (log_callback) log_callback("[REC] Capture already in progress...");
    return;
  }

  if (connected_devices_.empty()) {
    if (log_callback) log_callback("[NET] No cameras connected");
    return;
  }

  // Join previous capture thread if it exists
  if (capture_thread_.joinable()) {
    capture_thread_.join();
  }

  // Copy params by value for thread safety
  CaptureParams local_params = params;

  // Start async capture
  capture_thread_ = std::thread([this, session_path, local_params, log_callback]() {
    is_capturing_ = true;

    if (log_callback) log_callback("[REC] 🎬 Starting async capture...");

    // Call the synchronous method on background thread
    bool result = CaptureAllCameras(session_path, local_params);

    if (log_callback) {
      if (result) {
        log_callback("[REC] ✅ Async capture completed successfully!");
      } else {
        log_callback("[REC] ❌ Async capture completed with errors");
      }
    }
    
    is_capturing_ = false;
  });
}

bool CameraManager::CaptureCamera(const std::string& cameraId, const std::string& sessionPath) {
  try {
    auto deviceIt = connected_devices_.find(cameraId);
    auto bufferIt = connected_buffers_.find(cameraId);
    auto transferIt = connected_transfers_.find(cameraId);

    if (deviceIt == connected_devices_.end() || bufferIt == connected_buffers_.end() || transferIt == connected_transfers_.end()) {
      Log("[ERR] ❌ Missing components for camera " + cameraId);
      return false;
    }

    SapAcqDevice* device = deviceIt->second;
    SapBuffer* buffer = bufferIt->second;
    SapAcqDeviceToBuf* transfer = transferIt->second;

    // Null checks
    if (!device || !buffer || !transfer) {
      Log("[ERR] ❌ Null pointer in camera components for " + cameraId);
      return false;
    }

    try {
      // Find camera name
      std::string cameraName = "unknown";
      for (const auto& camera : discovered_cameras_) {
        if (camera.id == cameraId) {
          cameraName = camera.name;
          break;
        }
      }

      Log("[REC] 🔄 Triggering capture for " + cameraName + "...");

      // Trigger capture with error handling
      BOOL snapResult = FALSE;
      try {
        snapResult = transfer->Snap();
      } catch (const std::exception& e) {
        Log("[ERR] ❌ Exception during Snap() for " + cameraName + ": " + e.what());
        return false;
      } catch (...) {
        Log("[ERR] ❌ Unknown exception during Snap() for " + cameraName);
        return false;
      }

      if (!snapResult) {
        Log("[ERR] ❌ Failed to trigger capture for " + cameraName + " (Snap returned FALSE)");
        return false;
      }

      Log("[REC] ⏳ Waiting for capture completion...");

      // Wait for capture completion with error handling
      BOOL waitResult = FALSE;
      try {
        waitResult = transfer->Wait(5000);
      } catch (const std::exception& e) {
        Log("[ERR] ❌ Exception during Wait() for " + cameraName + ": " + e.what());
        return false;
      } catch (...) {
        Log("[ERR] ❌ Unknown exception during Wait() for " + cameraName);
        return false;
      }

      if (!waitResult) {
        Log("[ERR] ❌ Capture timeout for " + cameraName + " (Wait returned FALSE)");
        return false;
      }

      Log("[REC] 📸 Capture completed, processing image...");

      // Generate filename with timestamp
      auto now = std::chrono::system_clock::now();
      auto time_t = std::chrono::system_clock::to_time_t(now);
      std::stringstream ss;
      ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");

      std::string extension = capture_format_raw_ ? ".raw" : ".tiff";
      std::string filename = cameraName + "_" + ss.str() + extension;
      std::string fullPath = sessionPath + "/" + filename;

      Log("[REC] 💾 Saving image: " + filename);

      // Save image
      if (capture_format_raw_) {
        // Save as RAW with error handling
        BOOL saveResult = FALSE;
        try {
          saveResult = buffer->Save(fullPath.c_str(), "-format raw");
        } catch (const std::exception& e) {
          Log("[ERR] ❌ Exception saving RAW image: " + std::string(e.what()));
          return false;
        } catch (...) {
          Log("[ERR] ❌ Unknown exception saving RAW image");
          return false;
        }

        if (saveResult) {
          Log("[OK] ✅ RAW image saved: " + filename);
          return true;
        } else {
          Log("[ERR] ❌ Failed to save RAW image: " + filename);
          return false;
        }
      } else {
        // Perform color conversion before saving
        SapColorConversion colorConv(buffer);

        try {
          if (!colorConv.Enable(TRUE, color_config_.use_hardware)) {
            Log("[ERR] ❌ Failed to enable color conversion");
            return false;
          }
          if (!colorConv.Create()) {
            Log("[ERR] ❌ Failed to create color converter");
            return false;
          }

          // Output format mapping
          if (color_config_.color_output_format == std::string("RGB888")) {
            colorConv.SetOutputFormat(SapFormatRGB888);
          } else if (color_config_.color_output_format == std::string("RGB8888")) {
            colorConv.SetOutputFormat(SapFormatRGB8888);
          } else if (color_config_.color_output_format == std::string("RGB101010")) {
            colorConv.SetOutputFormat(SapFormatRGB101010);
          } else {
            colorConv.SetOutputFormat(SapFormatRGB888);
          }

          // Bayer align
          SapColorConversion::Align align = SapColorConversion::AlignRGGB;
          switch (color_config_.bayer_align) {
            case 0: align = SapColorConversion::AlignGBRG; break;
            case 1: align = SapColorConversion::AlignBGGR; break;
            case 2: align = SapColorConversion::AlignRGGB; break;
            case 3: align = SapColorConversion::AlignGRBG; break;
            case 4: align = SapColorConversion::AlignRGBG; break;
            case 5: align = SapColorConversion::AlignBGRG; break;
          }
          colorConv.SetAlign(align);

          // Method
          SapColorConversion::Method method = SapColorConversion::Method1;
          switch (color_config_.color_method) {
            case 1: method = SapColorConversion::Method1; break;
            case 2: method = SapColorConversion::Method2; break;
            case 3: method = SapColorConversion::Method3; break;
            case 4: method = SapColorConversion::Method4; break;
            case 5: method = SapColorConversion::Method5; break;
            case 6: method = SapColorConversion::Method6; break;
            case 7: method = SapColorConversion::Method7; break;
          }
          colorConv.SetMethod(method);

          // WB gain/offset and gamma
          SapDataFRGB wbGain(color_config_.wb_gain_r, color_config_.wb_gain_g, color_config_.wb_gain_b);
          colorConv.SetWBGain(wbGain);
          SapDataFRGB wbOff(color_config_.wb_offset_r, color_config_.wb_offset_g, color_config_.wb_offset_b);
          colorConv.SetWBOffset(wbOff);
          colorConv.SetGamma(color_config_.gamma);

          // Convert
          if (!colorConv.Convert()) {
            Log("[ERR] ❌ Color conversion failed");
            colorConv.Destroy();
            return false;
          }

          // Save converted buffer as TIFF
          SapBuffer* outBuf = colorConv.GetOutputBuffer();
          if (!outBuf) {
            Log("[ERR] ❌ No output buffer from color converter");
            colorConv.Destroy();
            return false;
          }

          BOOL saveOk = FALSE;
          try {
            saveOk = outBuf->Save(fullPath.c_str(), "-format tiff");
          } catch (const std::exception& e) {
            Log("[ERR] ❌ Exception saving TIFF: " + std::string(e.what()));
            colorConv.Destroy();
            return false;
          } catch (...) {
            Log("[ERR] ❌ Unknown exception saving TIFF");
            colorConv.Destroy();
            return false;
          }

          colorConv.Destroy();

          if (saveOk) {
            Log("[OK] ✅ TIFF image saved with color conversion: " + filename);
            return true;
          } else {
            Log("[ERR] ❌ Failed to save TIFF image: " + filename);
            return false;
          }

        } catch (const std::exception& e) {
          Log("[ERR] ❌ Exception during color conversion: " + std::string(e.what()));
          try { colorConv.Destroy(); } catch (...) {}
          return false;
        } catch (...) {
          Log("[ERR] ❌ Unknown exception during color conversion");
          try { colorConv.Destroy(); } catch (...) {}
          return false;
        }
      }

    } catch (const std::exception& e) {
      Log("[ERR] ❌ Exception in capture processing for " + cameraId + ": " + std::string(e.what()));
      return false;
    } catch (...) {
      Log("[ERR] ❌ Unknown exception in capture processing for " + cameraId);
      return false;
    }

  } catch (const std::exception& e) {
    Log("[ERR] ❌ Fatal exception capturing " + cameraId + ": " + std::string(e.what()));
    return false;
  } catch (...) {
    Log("[ERR] ❌ Fatal unknown exception capturing " + cameraId);
    return false;
  }
}

bool CameraManager::SaveImageFromBuffer(SapBuffer* buffer, const std::string& fullPath, const std::string& cameraName) {
  try {
    // Null check
    if (!buffer) {
      Log("[ERR] ❌ Null buffer for " + cameraName);
      return false;
    }

    Log("[REC] 💾 Saving image: " + fullPath);

    // Save image based on format
    if (capture_format_raw_) {
      // Save as RAW with error handling
      BOOL saveResult = FALSE;
      try {
        saveResult = buffer->Save(fullPath.c_str(), "-format raw");
      } catch (const std::exception& e) {
        Log("[ERR] ❌ Exception saving RAW image: " + std::string(e.what()));
        return false;
      } catch (...) {
        Log("[ERR] ❌ Unknown exception saving RAW image");
        return false;
      }

      if (saveResult) {
        Log("[OK] ✅ RAW image saved: " + fullPath);
        return true;
      } else {
        Log("[ERR] ❌ Failed to save RAW image: " + fullPath);
        return false;
      }
    } else {
      // Perform color conversion before saving
      SapColorConversion colorConv(buffer);

      try {
        if (!colorConv.Enable(TRUE, color_config_.use_hardware)) {
          Log("[ERR] ❌ Failed to enable color conversion");
          return false;
        }
        if (!colorConv.Create()) {
          Log("[ERR] ❌ Failed to create color converter");
          return false;
        }

        // Output format mapping
        if (color_config_.color_output_format == std::string("RGB888")) {
          colorConv.SetOutputFormat(SapFormatRGB888);
        } else if (color_config_.color_output_format == std::string("RGB8888")) {
          colorConv.SetOutputFormat(SapFormatRGB8888);
        } else if (color_config_.color_output_format == std::string("RGB101010")) {
          colorConv.SetOutputFormat(SapFormatRGB101010);
        } else {
          colorConv.SetOutputFormat(SapFormatRGB888);
        }

        // Bayer align
        SapColorConversion::Align align = SapColorConversion::AlignRGGB;
        switch (color_config_.bayer_align) {
          case 0: align = SapColorConversion::AlignGBRG; break;
          case 1: align = SapColorConversion::AlignBGGR; break;
          case 2: align = SapColorConversion::AlignRGGB; break;
          case 3: align = SapColorConversion::AlignGRBG; break;
          case 4: align = SapColorConversion::AlignRGBG; break;
          case 5: align = SapColorConversion::AlignBGRG; break;
        }
        colorConv.SetAlign(align);

        // Method
        SapColorConversion::Method method = SapColorConversion::Method1;
        switch (color_config_.color_method) {
          case 1: method = SapColorConversion::Method1; break;
          case 2: method = SapColorConversion::Method2; break;
          case 3: method = SapColorConversion::Method3; break;
          case 4: method = SapColorConversion::Method4; break;
          case 5: method = SapColorConversion::Method5; break;
          case 6: method = SapColorConversion::Method6; break;
          case 7: method = SapColorConversion::Method7; break;
        }
        colorConv.SetMethod(method);

        // WB gain/offset and gamma
        SapDataFRGB wbGain(color_config_.wb_gain_r, color_config_.wb_gain_g, color_config_.wb_gain_b);
        colorConv.SetWBGain(wbGain);
        SapDataFRGB wbOff(color_config_.wb_offset_r, color_config_.wb_offset_g, color_config_.wb_offset_b);
        colorConv.SetWBOffset(wbOff);
        colorConv.SetGamma(color_config_.gamma);

        // Convert
        if (!colorConv.Convert()) {
          Log("[ERR] ❌ Color conversion failed");
          colorConv.Destroy();
          return false;
        }

        // Save converted buffer as TIFF
        SapBuffer* outBuf = colorConv.GetOutputBuffer();
        if (!outBuf) {
          Log("[ERR] ❌ No output buffer from color converter");
          colorConv.Destroy();
          return false;
        }

        BOOL saveOk = FALSE;
        try {
          saveOk = outBuf->Save(fullPath.c_str(), "-format tiff");
        } catch (const std::exception& e) {
          Log("[ERR] ❌ Exception saving TIFF: " + std::string(e.what()));
          colorConv.Destroy();
          return false;
        } catch (...) {
          Log("[ERR] ❌ Unknown exception saving TIFF");
          colorConv.Destroy();
          return false;
        }

        colorConv.Destroy();

        if (saveOk) {
          Log("[OK] ✅ TIFF image saved with color conversion: " + fullPath);
          return true;
        } else {
          Log("[ERR] ❌ Failed to save TIFF image: " + fullPath);
          return false;
        }

      } catch (const std::exception& e) {
        Log("[ERR] ❌ Exception during color conversion: " + std::string(e.what()));
        try { colorConv.Destroy(); } catch (...) {}
        return false;
      } catch (...) {
        Log("[ERR] ❌ Unknown exception during color conversion");
        try { colorConv.Destroy(); } catch (...) {}
        return false;
      }
    }

  } catch (const std::exception& e) {
    Log("[ERR] ❌ Exception saving image for " + cameraName + ": " + std::string(e.what()));
    return false;
  } catch (...) {
    Log("[ERR] ❌ Unknown exception saving image for " + cameraName);
    return false;
  }
}

bool CameraManager::ApplySafeParameter(SapAcqDevice* device, const std::string& cameraId, const std::string& featureName, const std::string& value) {
  // Whitelist of known-good GenICam features with proper type conversion
  // Only parameters verified to work with Nano-C4020 cameras

  try {
    // Null check
    if (!device) {
      Log("[PARAM] ✗ Device is null for " + cameraId);
      return false;
    }

    // Check if feature is available first
    BOOL isAvailable = FALSE;
    try {
      if (!device->IsFeatureAvailable(featureName.c_str(), &isAvailable) || !isAvailable) {
        Log("[PARAM] ⊘ " + featureName + " not available on " + cameraId + ", skipping");
        return false;
      }
    } catch (const std::exception& e) {
      Log("[PARAM] ✗ Exception checking availability of " + featureName + " on " + cameraId + ": " + e.what());
      return false;
    } catch (...) {
      Log("[PARAM] ✗ Unknown exception checking availability of " + featureName + " on " + cameraId);
      return false;
    }

    BOOL result = FALSE;

    try {
      // ExposureTime - detect feature type and use appropriate overload
      if (featureName == "ExposureTime" || featureName == "ExposureTimeAbs") {
        // Query feature type to determine correct data type
        SapFeature feature(device->GetLocation());
        bool featureCreated = feature.Create();
        bool gotFeatureInfo = featureCreated && device->GetFeatureInfo(featureName.c_str(), &feature);

        if (gotFeatureInfo) {
          SapFeature::Type featureType;
          if (feature.GetType(&featureType)) {
            switch (featureType) {
              case SapFeature::TypeFloat:
              case SapFeature::TypeDouble: {
                double doubleValue = std::stod(value);
                result = device->SetFeatureValue(featureName.c_str(), doubleValue);
                Log("[PARAM] Using double type for " + featureName + " = " + std::to_string(doubleValue));
                break;
              }
              case SapFeature::TypeInt32:
              case SapFeature::TypeInt64:
              default: {
                INT64 intValue = std::stoll(value);
                result = device->SetFeatureValue(featureName.c_str(), intValue);
                Log("[PARAM] Using INT64 type for " + featureName + " = " + std::to_string(intValue));
                break;
              }
            }
          } else {
            // Fallback to INT64 if type query fails
            INT64 intValue = std::stoll(value);
            result = device->SetFeatureValue(featureName.c_str(), intValue);
            Log("[PARAM] Type query failed, using INT64 fallback for " + featureName);
          }
        } else {
          // Fallback to INT64 if feature info query fails
          INT64 intValue = std::stoll(value);
          result = device->SetFeatureValue(featureName.c_str(), intValue);
          Log("[PARAM] Feature info query failed, using INT64 fallback for " + featureName);
        }

        // Clean up SapFeature object
        if (featureCreated) {
          feature.Destroy();
        }
      }
      // Float/Double parameters
      else if (featureName == "Gain" || featureName == "GainRaw") {
        double doubleValue = std::stod(value);
        result = device->SetFeatureValue(featureName.c_str(), doubleValue);
      }
      // White balance parameters - SKIP these as they cause crashes
      else if (featureName == "BalanceRatioRed" ||
               featureName == "BalanceRatioGreen" ||
               featureName == "BalanceRatioBlue" ||
               featureName == "BalanceWhiteAuto" ||
               featureName == "WhiteBalanceRed" ||
               featureName == "WhiteBalanceGreen" ||
               featureName == "WhiteBalanceBlue") {
        Log("[PARAM] ⊘ White balance parameters not supported by Nano-C4020, skipping " + featureName);
        return false;
      }
      // Gamma - known to not be supported
      else if (featureName == "Gamma") {
        Log("[PARAM] ⊘ Gamma not supported by Nano-C4020, skipping");
        return false;
      }
      // Packet size/delay (integer)
      else if (featureName == "GevSCPSPacketSize" || featureName == "PacketSize") {
        INT64 intValue = std::stoll(value);
        result = device->SetFeatureValue(featureName.c_str(), intValue);
      }
      else if (featureName == "GevSCPD" || featureName == "PacketDelay") {
        INT64 intValue = std::stoll(value);
        result = device->SetFeatureValue(featureName.c_str(), intValue);
      }
      // Unknown parameter - DO NOT try, just skip
      else {
        Log("[PARAM] ⊘ Unknown parameter " + featureName + ", skipping for safety");
        return false;
      }
    } catch (const std::invalid_argument& e) {
      Log("[PARAM] ✗ Invalid value format for " + featureName + " = " + value + ": " + e.what());
      return false;
    } catch (const std::out_of_range& e) {
      Log("[PARAM] ✗ Value out of range for " + featureName + " = " + value + ": " + e.what());
      return false;
    } catch (const std::exception& e) {
      Log("[PARAM] ✗ Exception setting " + featureName + " = " + value + " on " + cameraId + ": " + e.what());
      return false;
    } catch (...) {
      Log("[PARAM] ✗ Unknown exception setting " + featureName + " = " + value + " on " + cameraId);
      return false;
    }

    if (result) {
      Log("[PARAM] ✓ " + featureName + " = " + value + " applied to " + cameraId);
      return true;
    } else {
      Log("[PARAM] ✗ Failed to apply " + featureName + " = " + value + " to " + cameraId + " (returned FALSE)");
      return false;
    }

  } catch (const std::exception& e) {
    Log("[PARAM] ✗ Fatal exception in ApplySafeParameter for " + featureName + " on " + cameraId + ": " + e.what());
    return false;
  } catch (...) {
    Log("[PARAM] ✗ Fatal unknown exception in ApplySafeParameter for " + featureName + " on " + cameraId);
    return false;
  }
}

void CameraManager::ApplyParameterToAllCameras(const std::string& featureName, const std::string& value) {
  try {
    if (connected_devices_.empty()) {
      Log("[PARAM] ⚠ No cameras connected, cannot apply " + featureName);
      return;
    }

    int successCount = 0;
    int totalCount = static_cast<int>(connected_devices_.size());
    int skippedCount = 0;

    for (auto& [cameraId, device] : connected_devices_) {
      try {
        if (device) {
          if (ApplySafeParameter(device, cameraId, featureName, value)) {
            successCount++;
          } else {
            skippedCount++;
          }
        } else {
          Log("[PARAM] ⚠ Null device pointer for camera " + cameraId);
          skippedCount++;
        }
      } catch (const std::exception& e) {
        Log("[PARAM] ✗ Exception applying " + featureName + " to " + cameraId + ": " + e.what());
        skippedCount++;
      } catch (...) {
        Log("[PARAM] ✗ Unknown exception applying " + featureName + " to " + cameraId);
        skippedCount++;
      }
    }

    if (successCount == totalCount) {
      Log("[PARAM] ✓ " + featureName + " = " + value + " applied to all " + std::to_string(totalCount) + " cameras");
    } else if (skippedCount == totalCount) {
      Log("[PARAM] ⚠ " + featureName + " skipped for all cameras (not supported or error)");
    } else {
      Log("[PARAM] ⚠ " + featureName + " applied to " + std::to_string(successCount) + "/" + std::to_string(totalCount) + " cameras (" + std::to_string(skippedCount) + " skipped)");
    }

  } catch (const std::exception& e) {
    Log("[PARAM] ✗ Fatal exception in ApplyParameterToAllCameras for " + featureName + ": " + e.what());
  } catch (...) {
    Log("[PARAM] ✗ Fatal unknown exception in ApplyParameterToAllCameras for " + featureName);
  }
}

void CameraManager::ApplyParameterToCamera(const std::string& cameraId, const std::string& featureName, const std::string& value) {
  try {
    auto deviceIt = connected_devices_.find(cameraId);
    if (deviceIt != connected_devices_.end()) {
      SapAcqDevice* device = deviceIt->second;
      if (device) {
        try {
          ApplySafeParameter(device, cameraId, featureName, value);
        } catch (const std::exception& e) {
          Log("[PARAM] ✗ Exception in ApplyParameterToCamera for " + cameraId + ": " + e.what());
        } catch (...) {
          Log("[PARAM] ✗ Unknown exception in ApplyParameterToCamera for " + cameraId);
        }
      } else {
        Log("[PARAM] ✗ Null device pointer for camera " + cameraId);
      }
    } else {
      Log("[PARAM] ⚠ Camera " + cameraId + " not found");
    }
  } catch (const std::exception& e) {
    Log("[PARAM] ✗ Fatal exception in ApplyParameterToCamera: " + std::string(e.what()));
  } catch (...) {
    Log("[PARAM] ✗ Fatal unknown exception in ApplyParameterToCamera");
  }
}

void CameraManager::DetectCameraResolution() {
  if (connected_devices_.empty()) {
    Log("[PARAM] No cameras connected for resolution detection");
    return;
  }
  
  // Get resolution from the first connected camera
  auto firstCamera = connected_devices_.begin();
  SapAcqDevice* device = firstCamera->second;
  
  if (!device) {
    Log("[PARAM] Invalid device handle");
    return;
  }
  
  try {
    int maxWidth = 0, maxHeight = 0;
    int currentWidth = 0, currentHeight = 0;
    
    // Get current dimensions
    if (device->GetFeatureValue("Width", &currentWidth)) {
      params_.width = currentWidth;
      Log("[PARAM] Current width detected: " + std::to_string(currentWidth));
    }
    
    if (device->GetFeatureValue("Height", &currentHeight)) {
      params_.height = currentHeight;
      Log("[PARAM] Current height detected: " + std::to_string(currentHeight));
    }
    
    // Try to get maximum dimensions
    if (device->GetFeatureValue("WidthMax", &maxWidth)) {
      params_.max_width = maxWidth;
      Log("[PARAM] Maximum width detected: " + std::to_string(maxWidth));
    } else {
      // Fallback: try SensorWidth
      if (device->GetFeatureValue("SensorWidth", &maxWidth)) {
        params_.max_width = maxWidth;
        Log("[PARAM] Sensor width detected: " + std::to_string(maxWidth));
      }
    }
    
    if (device->GetFeatureValue("HeightMax", &maxHeight)) {
      params_.max_height = maxHeight;
      Log("[PARAM] Maximum height detected: " + std::to_string(maxHeight));
    } else {
      // Fallback: try SensorHeight
      if (device->GetFeatureValue("SensorHeight", &maxHeight)) {
        params_.max_height = maxHeight;
        Log("[PARAM] Sensor height detected: " + std::to_string(maxHeight));
      }
    }
    
    Log("[PARAM] Resolution detection complete - Max: " + 
        std::to_string(params_.max_width) + "x" + std::to_string(params_.max_height));
    
  } catch (const std::exception& e) {
    Log("[PARAM] Error detecting resolution: " + std::string(e.what()));
  }
}

bool CameraManager::IsConnected(const std::string& camera_id) const {
  return connected_devices_.find(camera_id) != connected_devices_.end();
}

void CameraManager::ApplyCameraOrdering(const ::CameraOrderSettings& order_settings) {
  if (!order_settings.use_custom_ordering) {
    // Reset all display positions to use discovery order
    for (auto& camera : discovered_cameras_) {
      camera.displayPosition = -1;
    }
    return;
  }

  // Apply custom ordering from settings
  for (auto& camera : discovered_cameras_) {
    int position = order_settings.GetDisplayPosition(camera.serialNumber);
    camera.displayPosition = position;

    // Update camera name based on position
    if (position >= 0) {
      std::string indexStr = std::to_string(position + 1);  // 1-based for display
      if (indexStr.length() == 1) indexStr = "0" + indexStr;
      camera.name = "cam_" + indexStr;
    }
  }

  Log("[ORDER] Applied custom camera ordering");
}

void CameraManager::ReorderCamera(int fromIndex, int toIndex) {
  std::lock_guard<std::mutex> lock(camera_mutex_);

  if (fromIndex < 0 || fromIndex >= static_cast<int>(discovered_cameras_.size()) ||
      toIndex < 0 || toIndex >= static_cast<int>(discovered_cameras_.size()) ||
      fromIndex == toIndex) {
    return;
  }

  // Move camera from fromIndex to toIndex
  CameraInfo camera = discovered_cameras_[fromIndex];
  discovered_cameras_.erase(discovered_cameras_.begin() + fromIndex);
  discovered_cameras_.insert(discovered_cameras_.begin() + toIndex, camera);

  // Update all camera names based on new positions
  for (size_t i = 0; i < discovered_cameras_.size(); ++i) {
    std::string indexStr = std::to_string(i + 1);  // 1-based for display
    if (indexStr.length() == 1) indexStr = "0" + indexStr;
    discovered_cameras_[i].name = "cam_" + indexStr;
    discovered_cameras_[i].displayPosition = static_cast<int>(i);
  }

  Log("[ORDER] Reordered cameras: moved " + std::to_string(fromIndex) + " to " + std::to_string(toIndex));
}

void CameraManager::SetCameraEnabled(int index, bool enabled) {
  std::lock_guard<std::mutex> lock(disabled_cameras_mutex_);
  if (enabled) {
    disabled_cameras_.erase(index);
  } else {
    disabled_cameras_.insert(index);
  }
  Log("[CAM] Camera " + std::to_string(index) + " " + (enabled ? "enabled" : "disabled") + " for capture");
}

bool CameraManager::IsCameraEnabled(int index) const {
  std::lock_guard<std::mutex> lock(disabled_cameras_mutex_);
  return disabled_cameras_.find(index) == disabled_cameras_.end();
}

void CameraManager::EnableAllCameras() {
  std::lock_guard<std::mutex> lock(disabled_cameras_mutex_);
  disabled_cameras_.clear();
  Log("[CAM] All cameras enabled for capture");
}

int CameraManager::GetEnabledCameraCount() const {
  std::lock_guard<std::mutex> lock(disabled_cameras_mutex_);
  int totalCameras = static_cast<int>(connected_devices_.size());
  int disabledCount = 0;
  for (int idx : disabled_cameras_) {
    if (idx < totalCameras) disabledCount++;
  }
  return totalCameras - disabledCount;
}

std::vector<CameraInfo> CameraManager::GetOrderedCameras() const {
  std::vector<CameraInfo> ordered = discovered_cameras_;

  // Sort by display position (cameras without position go to end)
  std::sort(ordered.begin(), ordered.end(), [](const CameraInfo& a, const CameraInfo& b) {
    // Cameras with position set come first
    if (a.displayPosition >= 0 && b.displayPosition < 0) return true;
    if (a.displayPosition < 0 && b.displayPosition >= 0) return false;

    // Both have positions or both don't
    if (a.displayPosition >= 0 && b.displayPosition >= 0) {
      return a.displayPosition < b.displayPosition;
    }

    // Neither has position, maintain discovery order (by id)
    return std::stoi(a.id) < std::stoi(b.id);
  });

  return ordered;
}

std::string CameraManager::GetOrderedCameraName(const std::string& serial_number, int fallback_index) const {
  // Find camera by serial number
  for (const auto& camera : discovered_cameras_) {
    if (camera.serialNumber == serial_number) {
      if (camera.displayPosition >= 0) {
        // Use display position
        std::string indexStr = std::to_string(camera.displayPosition + 1);
        if (indexStr.length() == 1) indexStr = "0" + indexStr;
        return "cam_" + indexStr;
      }
      break;
    }
  }

  // Fallback to provided index
  std::string indexStr = std::to_string(fallback_index);
  if (indexStr.length() == 1) indexStr = "0" + indexStr;
  return "cam_" + indexStr;
}

void CameraManager::Log(const std::string& message) {
  if (log_callback_) {
    log_callback_(message);
  }
}

}  // namespace SaperaCapturePro