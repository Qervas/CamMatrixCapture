#include "CameraManager.hpp"
#include <chrono>
#include <filesystem>
#include <sstream>
#include <iomanip>

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
      
      // Enumerate acquisition devices
      for (int resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++) {
        try {
          // Create acquisition device temporarily for discovery
          auto acqDevice = new SapAcqDevice(serverName, resourceIndex);
          if (!acqDevice->Create()) {
            Log("[NET] Failed to create device " + std::to_string(resourceIndex));
            delete acqDevice;
            continue;
          }
          
          // Get device information
          char buffer[512];
          
          CameraInfo camera;
          camera.id = std::to_string(cameraIndex);
          camera.serverName = serverName;
          camera.resourceIndex = resourceIndex;
          
          // Get serial number
          if (acqDevice->GetFeatureValue("DeviceSerialNumber", buffer, sizeof(buffer))) {
            camera.serialNumber = std::string(buffer);
          } else {
            camera.serialNumber = "Unknown_" + std::to_string(cameraIndex);
          }
          
          // Get model name
          if (acqDevice->GetFeatureValue("DeviceModelName", buffer, sizeof(buffer))) {
            camera.modelName = std::string(buffer);
          } else {
            camera.modelName = "Unknown_Model";
          }
          
          // Create camera name for neural rendering
          std::string indexStr = std::to_string(cameraIndex);
          if (indexStr.length() == 1) indexStr = "0" + indexStr;
          camera.name = "cam_" + indexStr;
          camera.isConnected = false;
          camera.status = CameraStatus::Disconnected;
          camera.type = CameraType::Industrial;
          
          temp_cameras.push_back(camera);
          Log("[OK] " + camera.name + ": " + camera.serialNumber + " (" + camera.modelName + ")");
          
          // Cleanup discovery device
          acqDevice->Destroy();
          delete acqDevice;
          
          cameraIndex++;
          
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
        
        // Apply exposure time setting
        std::string exposureStr = std::to_string(exposure_time_);
        if (!acqDevice->SetFeatureValue("ExposureTime", exposureStr.c_str())) {
          Log("[NET] Warning: Failed to set exposure time for " + camera.name);
        }
        
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
  
  // Destroy all connected devices and buffers
  int destroyed_count = 0;
  for (auto& [id, device] : connected_devices_) {
    if (device) {
      try {
        device->Destroy();
        delete device;
        destroyed_count++;
        Log("[OK] Disconnected device: " + id);
      } catch (const std::exception& e) {
        Log("[ERROR] Failed to destroy device " + id + ": " + e.what());
      }
    }
  }
  
  for (auto& [id, buffer] : connected_buffers_) {
    if (buffer) {
      try {
        buffer->Destroy();
        delete buffer;
      } catch (const std::exception& e) {
        Log("[ERROR] Failed to destroy buffer " + id + ": " + e.what());
      }
    }
  }
  
  for (auto& [id, transfer] : connected_transfers_) {
    if (transfer) {
      try {
        transfer->Destroy();
        delete transfer;
      } catch (const std::exception& e) {
        Log("[ERROR] Failed to destroy transfer " + id + ": " + e.what());
      }
    }
  }
  
  // Clear all connected devices and buffers
  connected_devices_.clear();
  connected_buffers_.clear();
  connected_transfers_.clear();
  
  // Update camera connection status in discovered_cameras_ list
  for (auto& camera : discovered_cameras_) {
    camera.isConnected = false;
    camera.status = CameraStatus::Disconnected;
  }
  
  Log("[OK] Successfully disconnected " + std::to_string(destroyed_count) + "/" + std::to_string(device_count) + " cameras");
}

bool CameraManager::CaptureAllCameras(const std::string& session_path, bool sequential, int delay_ms) {
  if (connected_devices_.empty()) {
    Log("[NET] No cameras connected");
    return false;
  }
  
  // Create session directory
  std::filesystem::create_directories(session_path);
  
  Log("[REC] 🎬 DIRECT CAPTURE starting...");
  Log("[IMG] Session path: " + session_path);
  
  auto startTime = std::chrono::high_resolution_clock::now();
  
  bool allSuccess = true;
  int successCount = 0;
  int totalCameras = static_cast<int>(connected_devices_.size());
  
  Log("[REC] 📸 Capturing from " + std::to_string(totalCameras) + " cameras...");
  
  if (sequential) {
    // Sequential capture with delays
    for (const auto& camera : discovered_cameras_) {
      std::string cameraId = camera.id;
      
      if (connected_devices_.find(cameraId) != connected_devices_.end()) {
        Log("[REC] 📷 Capturing " + camera.name + " (" + camera.serialNumber + ")");
        
        if (CaptureCamera(cameraId, session_path)) {
          successCount++;
          Log("[OK] ✅ " + camera.name + " captured successfully");
        } else {
          allSuccess = false;
          Log("[ERR] ❌ " + camera.name + " capture failed");
        }
        
        // Add delay between cameras (except for the last one)
        if (successCount < totalCameras) {
          std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
      }
    }
  } else {
    // Simultaneous capture
    for (const auto& camera : discovered_cameras_) {
      std::string cameraId = camera.id;
      
      if (connected_devices_.find(cameraId) != connected_devices_.end()) {
        Log("[REC] 📷 Capturing " + camera.name + " (" + camera.serialNumber + ")");
        
        if (CaptureCamera(cameraId, session_path)) {
          successCount++;
          Log("[OK] ✅ " + camera.name + " captured successfully");
        } else {
          allSuccess = false;
          Log("[ERR] ❌ " + camera.name + " capture failed");
        }
      }
    }
  }
  
  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
  
  Log("[REC] 🏁 Capture completed in " + std::to_string(duration.count()) + "ms");
  Log("[REC] 📊 Success rate: " + std::to_string(successCount) + "/" + std::to_string(totalCameras) + " cameras");
  
  return allSuccess;
}

void CameraManager::CaptureAllCamerasAsync(const std::string& session_path, bool sequential, int delay_ms, std::function<void(const std::string&)> log_callback) {
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
  
  // Start async capture
  capture_thread_ = std::thread([this, session_path, sequential, delay_ms, log_callback]() {
    is_capturing_ = true;
    
    if (log_callback) log_callback("[REC] 🎬 Starting async capture...");
    
    // Call the synchronous method on background thread
    bool result = CaptureAllCameras(session_path, sequential, delay_ms);
    
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
    
    // Trigger capture
    if (!transfer->Snap()) {
      Log("[ERR] ❌ Failed to trigger capture for " + cameraName);
      return false;
    }
    
    Log("[REC] ⏳ Waiting for capture completion...");
    
    // Wait for capture completion
    if (!transfer->Wait(5000)) {
      Log("[ERR] ❌ Capture timeout for " + cameraName);
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
      // Save as RAW
      if (buffer->Save(fullPath.c_str(), "-format raw")) {
        Log("[OK] ✅ RAW image saved: " + filename);
        return true;
      } else {
        Log("[ERR] ❌ Failed to save RAW image: " + filename);
        return false;
      }
    } else {
      // Save as TIFF
      if (buffer->Save(fullPath.c_str(), "-format tiff")) {
        Log("[OK] ✅ TIFF image saved: " + filename);
        return true;
      } else {
        Log("[ERR] ❌ Failed to save TIFF image: " + filename);
        return false;
      }
    }
    
  } catch (const std::exception& e) {
    Log("[ERR] ❌ Exception capturing " + cameraId + ": " + std::string(e.what()));
    return false;
  }
}

void CameraManager::ApplyParameterToAllCameras(const std::string& featureName, const std::string& value) {
  int successCount = 0;
  int totalCount = static_cast<int>(connected_devices_.size());
  
  for (auto& [cameraId, device] : connected_devices_) {
    if (device) {
      BOOL result = device->SetFeatureValue(featureName.c_str(), value.c_str());
      if (result) {
        successCount++;
        Log("[PARAM] ✓ " + featureName + " = " + value + " applied to " + cameraId);
      } else {
        Log("[PARAM] ✗ Failed to apply " + featureName + " = " + value + " to " + cameraId);
      }
    }
  }
  
  if (successCount == totalCount) {
    Log("[PARAM] ✓ " + featureName + " = " + value + " applied to all " + std::to_string(totalCount) + " cameras");
  } else {
    Log("[PARAM] ⚠ " + featureName + " applied to " + std::to_string(successCount) + "/" + std::to_string(totalCount) + " cameras");
  }
}

void CameraManager::ApplyParameterToCamera(const std::string& cameraId, const std::string& featureName, const std::string& value) {
  auto deviceIt = connected_devices_.find(cameraId);
  if (deviceIt != connected_devices_.end()) {
    SapAcqDevice* device = deviceIt->second;
    if (device && device->SetFeatureValue(featureName.c_str(), value.c_str())) {
      Log("[PARAM] " + featureName + " = " + value + " applied to " + cameraId);
    } else {
      Log("[PARAM] Failed to apply " + featureName + " to " + cameraId);
    }
  } else {
    Log("[PARAM] Camera " + cameraId + " not found");
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

void CameraManager::Log(const std::string& message) {
  if (log_callback_) {
    log_callback_(message);
  }
}

}  // namespace SaperaCapturePro