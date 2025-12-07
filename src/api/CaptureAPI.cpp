/**
 * CaptureAPI.cpp
 *
 * Implementation of C-style interface for Camera Matrix Capture backend.
 * Wraps C++ singleton managers for use by WinUI frontend via P/Invoke.
 */

#include "CaptureAPI.h"
#include "../hardware/CameraManager.hpp"
#include "../bluetooth/BluetoothManager.hpp"
#include "../utils/SettingsManager.hpp"
#include "../utils/SessionManager.hpp"

#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <vector>

// Use namespaces
using namespace SaperaCapturePro;

// ============================================================================
// Global State
// ============================================================================

static std::unique_ptr<SettingsManager> g_settingsManager;
static std::unique_ptr<SessionManager> g_sessionManager;
static std::mutex g_mutex;

// Callbacks (stored globally for C interop) - use C API types explicitly
static ::LogCallback g_logCallback = nullptr;
static ::ProgressCallback g_progressCallback = nullptr;
static ::DeviceDiscoveredCallback g_deviceDiscoveredCallback = nullptr;
static ::CaptureCompleteCallback g_captureCompleteCallback = nullptr;

// Capture state
static std::atomic<int> g_captureProgress{0};
static std::atomic<int> g_totalPositions{0};
static std::string g_lastSessionPath;
static std::atomic<int> g_lastImageCount{0};

// Turntable state
static std::string g_connectedTurntableId;
static std::atomic<float> g_currentAngle{0.0f};
static std::atomic<float> g_currentTilt{0.0f};

// ============================================================================
// Helper Functions
// ============================================================================

static void SafeLog(const std::string& message) {
    if (g_logCallback) {
        g_logCallback(message.c_str());
    }
}

static void SafeCopyString(const std::string& src, char* dest, int maxLen) {
    if (dest && maxLen > 0) {
        strncpy_s(dest, maxLen, src.c_str(), _TRUNCATE);
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

CAPTURE_API void CamMatrix_Initialize() {
    std::lock_guard<std::mutex> lock(g_mutex);

    // Initialize settings manager
    if (!g_settingsManager) {
        g_settingsManager = std::make_unique<SettingsManager>("settings.ini");
        g_settingsManager->Load();
    }

    // Initialize session manager
    if (!g_sessionManager) {
        auto& appSettings = g_settingsManager->GetAppSettings();
        g_sessionManager = std::make_unique<SessionManager>(appSettings.last_output_folder);
    }

    // Initialize Bluetooth manager
    BluetoothManager::GetInstance().Initialize();

    SafeLog("CamMatrix API initialized");
}

CAPTURE_API void CamMatrix_Shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);

    // Disconnect all cameras
    CameraManager::GetInstance().DisconnectAllCameras();

    // Disconnect Bluetooth
    BluetoothManager::GetInstance().DisconnectAllDevices();
    BluetoothManager::GetInstance().Shutdown();

    // Save settings
    if (g_settingsManager) {
        g_settingsManager->Save();
    }

    // Clear managers
    g_sessionManager.reset();
    g_settingsManager.reset();

    SafeLog("CamMatrix API shutdown");
}

// ============================================================================
// Camera Operations
// ============================================================================

CAPTURE_API void CamMatrix_DiscoverCameras() {
    CameraManager::GetInstance().DiscoverCameras([](const std::string& msg) {
        SafeLog(msg);
    });
}

CAPTURE_API void CamMatrix_ConnectAllCameras() {
    CameraManager::GetInstance().ConnectAllCameras([](const std::string& msg) {
        SafeLog(msg);
    });
}

CAPTURE_API void CamMatrix_DisconnectAllCameras() {
    CameraManager::GetInstance().DisconnectAllCameras();
}

CAPTURE_API int CamMatrix_GetDiscoveredCameraCount() {
    return static_cast<int>(CameraManager::GetInstance().GetDiscoveredCameras().size());
}

CAPTURE_API int CamMatrix_GetConnectedCameraCount() {
    return CameraManager::GetInstance().GetConnectedCount();
}

CAPTURE_API int CamMatrix_IsDiscovering() {
    return CameraManager::GetInstance().IsDiscovering() ? 1 : 0;
}

CAPTURE_API int CamMatrix_IsConnecting() {
    return CameraManager::GetInstance().IsConnecting() ? 1 : 0;
}

CAPTURE_API int CamMatrix_IsCapturing() {
    return CameraManager::GetInstance().IsCapturing() ? 1 : 0;
}

CAPTURE_API void CamMatrix_GetCameraName(int index, char* nameOut, int maxLen) {
    auto& cameras = CameraManager::GetInstance().GetDiscoveredCameras();
    if (index >= 0 && index < static_cast<int>(cameras.size())) {
        SafeCopyString(cameras[index].name, nameOut, maxLen);
    } else if (nameOut && maxLen > 0) {
        nameOut[0] = '\0';
    }
}

CAPTURE_API int CamMatrix_IsCameraConnected(int index) {
    auto& cameras = CameraManager::GetInstance().GetDiscoveredCameras();
    if (index >= 0 && index < static_cast<int>(cameras.size())) {
        return CameraManager::GetInstance().IsConnected(cameras[index].id) ? 1 : 0;
    }
    return 0;
}

// ============================================================================
// Capture Operations
// ============================================================================

CAPTURE_API void CamMatrix_StartCapture(const char* sessionName, int totalPositions, float angleStep) {
    if (!g_sessionManager) return;

    g_totalPositions = totalPositions;
    g_captureProgress = 0;

    // Create session
    std::string name = sessionName ? sessionName : "";
    g_sessionManager->StartNewSession(name);
    if (g_sessionManager->GetCurrentSession()) {
        g_lastSessionPath = g_sessionManager->GetCurrentSession()->base_path;
    }

    // Get capture params from settings
    CameraManager::CaptureParams params;
    if (g_settingsManager) {
        auto& camSettings = g_settingsManager->GetCameraSettings();
        params.parallel_groups = camSettings.parallel_capture_groups;
        params.group_delay_ms = camSettings.capture_delay_ms;
        params.stagger_delay_ms = camSettings.stagger_delay_ms;
    }

    // Start capture thread with turntable rotation
    std::thread captureThread([params, angleStep]() {
        auto& camMgr = CameraManager::GetInstance();
        auto& btMgr = BluetoothManager::GetInstance();

        for (int pos = 0; pos < g_totalPositions.load(); ++pos) {
            if (!camMgr.IsCapturing()) break;

            // Create position folder
            std::string posPath = g_lastSessionPath + "/pos_" + std::to_string(pos + 1);
            std::filesystem::create_directories(posPath);

            // Capture all cameras
            camMgr.CaptureAllCameras(posPath, params);

            g_captureProgress = pos + 1;
            if (g_progressCallback) {
                g_progressCallback(pos + 1, g_totalPositions.load());
            }

            // Rotate turntable for next position (if not last)
            if (pos < g_totalPositions.load() - 1 && !g_connectedTurntableId.empty()) {
                btMgr.RotateTurntable(g_connectedTurntableId, angleStep);
                g_currentAngle = g_currentAngle.load() + angleStep;

                // Wait for rotation to complete
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            }
        }

        // Calculate total images
        g_lastImageCount = g_captureProgress.load() * camMgr.GetConnectedCount();

        // Notify completion
        if (g_captureCompleteCallback) {
            g_captureCompleteCallback(1, g_lastSessionPath.c_str());
        }

        SafeLog("Capture sequence completed");
    });
    captureThread.detach();
}

CAPTURE_API void CamMatrix_StopCapture() {
    // Signal stop by setting progress to total (capture loop checks IsCapturing)
    g_captureProgress = g_totalPositions.load();
}

CAPTURE_API void CamMatrix_CaptureOnce(const char* sessionPath) {
    if (!sessionPath) return;

    CameraManager::CaptureParams params;
    if (g_settingsManager) {
        auto& camSettings = g_settingsManager->GetCameraSettings();
        params.parallel_groups = camSettings.parallel_capture_groups;
        params.group_delay_ms = camSettings.capture_delay_ms;
        params.stagger_delay_ms = camSettings.stagger_delay_ms;
    }

    CameraManager::GetInstance().CaptureAllCameras(sessionPath, params);
}

CAPTURE_API int CamMatrix_GetCaptureProgress() {
    return g_captureProgress.load();
}

CAPTURE_API int CamMatrix_GetTotalPositions() {
    return g_totalPositions.load();
}

// ============================================================================
// Bluetooth / Turntable Operations
// ============================================================================

CAPTURE_API void CamMatrix_StartBluetoothScan() {
    auto& btMgr = BluetoothManager::GetInstance();

    // Set up device discovered callback
    btMgr.SetDeviceDiscoveredCallback([](const std::string& id, const std::string& name) {
        if (g_deviceDiscoveredCallback) {
            g_deviceDiscoveredCallback(id.c_str(), name.c_str());
        }
    });

    btMgr.StartScanning();
}

CAPTURE_API void CamMatrix_StopBluetoothScan() {
    BluetoothManager::GetInstance().StopScanning();
}

CAPTURE_API int CamMatrix_IsBluetoothScanning() {
    return BluetoothManager::GetInstance().IsScanning() ? 1 : 0;
}

CAPTURE_API int CamMatrix_GetBluetoothDeviceCount() {
    return static_cast<int>(BluetoothManager::GetInstance().GetDiscoveredDevices().size());
}

CAPTURE_API void CamMatrix_GetBluetoothDevice(int index, char* idOut, int idMaxLen, char* nameOut, int nameMaxLen) {
    auto devices = BluetoothManager::GetInstance().GetDiscoveredDevices();
    if (index >= 0 && index < static_cast<int>(devices.size())) {
        SafeCopyString(devices[index].first, idOut, idMaxLen);
        SafeCopyString(devices[index].second, nameOut, nameMaxLen);
    } else {
        if (idOut && idMaxLen > 0) idOut[0] = '\0';
        if (nameOut && nameMaxLen > 0) nameOut[0] = '\0';
    }
}

CAPTURE_API int CamMatrix_ConnectBluetooth(const char* deviceId) {
    if (!deviceId) return 0;

    bool success = BluetoothManager::GetInstance().ConnectToDevice(deviceId);
    if (success) {
        g_connectedTurntableId = deviceId;
        g_currentAngle = 0.0f;
        g_currentTilt = 0.0f;

        // Save to settings for Quick Connect
        if (g_settingsManager) {
            auto& appSettings = g_settingsManager->GetAppSettings();
            appSettings.last_bluetooth_device_id = deviceId;

            // Find device name
            auto devices = BluetoothManager::GetInstance().GetDiscoveredDevices();
            for (const auto& dev : devices) {
                if (dev.first == deviceId) {
                    appSettings.last_bluetooth_device_name = dev.second;
                    break;
                }
            }
        }
    }
    return success ? 1 : 0;
}

CAPTURE_API void CamMatrix_DisconnectBluetooth() {
    if (!g_connectedTurntableId.empty()) {
        BluetoothManager::GetInstance().DisconnectDevice(g_connectedTurntableId);
        g_connectedTurntableId.clear();
    }
}

CAPTURE_API int CamMatrix_IsBluetoothConnected() {
    return BluetoothManager::GetInstance().IsConnected() ? 1 : 0;
}

CAPTURE_API int CamMatrix_RotateTurntable(float angle) {
    if (g_connectedTurntableId.empty()) return 0;
    bool success = BluetoothManager::GetInstance().RotateTurntable(g_connectedTurntableId, angle);
    if (success) {
        g_currentAngle = g_currentAngle.load() + angle;
    }
    return success ? 1 : 0;
}

CAPTURE_API int CamMatrix_TiltTurntable(float angle) {
    if (g_connectedTurntableId.empty()) return 0;
    bool success = BluetoothManager::GetInstance().TiltTurntable(g_connectedTurntableId, angle);
    if (success) {
        g_currentTilt = angle;
    }
    return success ? 1 : 0;
}

CAPTURE_API int CamMatrix_ReturnToZero() {
    if (g_connectedTurntableId.empty()) return 0;
    bool success = BluetoothManager::GetInstance().ReturnToZero(g_connectedTurntableId);
    if (success) {
        g_currentAngle = 0.0f;
        g_currentTilt = 0.0f;
    }
    return success ? 1 : 0;
}

CAPTURE_API int CamMatrix_StopTurntable() {
    if (g_connectedTurntableId.empty()) return 0;
    return BluetoothManager::GetInstance().StopRotation(g_connectedTurntableId) ? 1 : 0;
}

CAPTURE_API float CamMatrix_GetCurrentAngle() {
    return g_currentAngle.load();
}

CAPTURE_API float CamMatrix_GetCurrentTilt() {
    return g_currentTilt.load();
}

// ============================================================================
// Settings Operations
// ============================================================================

CAPTURE_API void CamMatrix_LoadSettings() {
    if (g_settingsManager) {
        g_settingsManager->Load();
    }
}

CAPTURE_API void CamMatrix_SaveSettings() {
    if (g_settingsManager) {
        g_settingsManager->Save();
    }
}

CAPTURE_API int CamMatrix_GetExposureTime() {
    if (g_settingsManager) {
        return g_settingsManager->GetCameraSettings().exposure_time;
    }
    return 40000;
}

CAPTURE_API void CamMatrix_SetExposureTime(int microseconds) {
    if (g_settingsManager) {
        g_settingsManager->GetCameraSettings().exposure_time = microseconds;
        CameraManager::GetInstance().SetExposureTime(microseconds);
        CameraManager::GetInstance().ApplyParameterToAllCameras("ExposureTime", std::to_string(microseconds));
    }
}

CAPTURE_API float CamMatrix_GetGain() {
    if (g_settingsManager) {
        return g_settingsManager->GetCameraSettings().gain;
    }
    return 1.0f;
}

CAPTURE_API void CamMatrix_SetGain(float db) {
    if (g_settingsManager) {
        g_settingsManager->GetCameraSettings().gain = db;
        CameraManager::GetInstance().ApplyParameterToAllCameras("Gain", std::to_string(db));
    }
}

CAPTURE_API float CamMatrix_GetWhiteBalanceRed() {
    if (g_settingsManager) {
        return g_settingsManager->GetCameraSettings().white_balance_red;
    }
    return 1.0f;
}

CAPTURE_API void CamMatrix_SetWhiteBalanceRed(float value) {
    if (g_settingsManager) {
        g_settingsManager->GetCameraSettings().white_balance_red = value;
    }
}

CAPTURE_API float CamMatrix_GetWhiteBalanceGreen() {
    if (g_settingsManager) {
        return g_settingsManager->GetCameraSettings().white_balance_green;
    }
    return 1.0f;
}

CAPTURE_API void CamMatrix_SetWhiteBalanceGreen(float value) {
    if (g_settingsManager) {
        g_settingsManager->GetCameraSettings().white_balance_green = value;
    }
}

CAPTURE_API float CamMatrix_GetWhiteBalanceBlue() {
    if (g_settingsManager) {
        return g_settingsManager->GetCameraSettings().white_balance_blue;
    }
    return 1.0f;
}

CAPTURE_API void CamMatrix_SetWhiteBalanceBlue(float value) {
    if (g_settingsManager) {
        g_settingsManager->GetCameraSettings().white_balance_blue = value;
    }
}

CAPTURE_API int CamMatrix_GetParallelGroups() {
    if (g_settingsManager) {
        return g_settingsManager->GetCameraSettings().parallel_capture_groups;
    }
    return 1;
}

CAPTURE_API void CamMatrix_SetParallelGroups(int groups) {
    if (g_settingsManager) {
        g_settingsManager->GetCameraSettings().parallel_capture_groups = groups;
    }
}

CAPTURE_API int CamMatrix_GetStaggerDelay() {
    if (g_settingsManager) {
        return g_settingsManager->GetCameraSettings().stagger_delay_ms;
    }
    return 150;
}

CAPTURE_API void CamMatrix_SetStaggerDelay(int milliseconds) {
    if (g_settingsManager) {
        g_settingsManager->GetCameraSettings().stagger_delay_ms = milliseconds;
    }
}

CAPTURE_API int CamMatrix_GetCaptureFormatRaw() {
    return CameraManager::GetInstance().GetCaptureFormat() ? 1 : 0;
}

CAPTURE_API void CamMatrix_SetCaptureFormatRaw(int useRaw) {
    CameraManager::GetInstance().SetCaptureFormat(useRaw != 0);
}

CAPTURE_API void CamMatrix_GetOutputPath(char* pathOut, int maxLen) {
    if (g_settingsManager) {
        SafeCopyString(g_settingsManager->GetAppSettings().last_output_folder, pathOut, maxLen);
    } else if (pathOut && maxLen > 0) {
        SafeCopyString("neural_dataset", pathOut, maxLen);
    }
}

CAPTURE_API void CamMatrix_SetOutputPath(const char* path) {
    if (g_settingsManager && path) {
        g_settingsManager->GetAppSettings().last_output_folder = path;
        if (g_sessionManager) {
            g_sessionManager->SetOutputPath(path);
        }
    }
}

// ============================================================================
// Callbacks
// ============================================================================

CAPTURE_API void CamMatrix_SetLogCallback(::LogCallback callback) {
    g_logCallback = callback;
}

CAPTURE_API void CamMatrix_SetProgressCallback(::ProgressCallback callback) {
    g_progressCallback = callback;
}

CAPTURE_API void CamMatrix_SetDeviceDiscoveredCallback(::DeviceDiscoveredCallback callback) {
    g_deviceDiscoveredCallback = callback;
}

CAPTURE_API void CamMatrix_SetCaptureCompleteCallback(::CaptureCompleteCallback callback) {
    g_captureCompleteCallback = callback;
}

// ============================================================================
// Session Info
// ============================================================================

CAPTURE_API void CamMatrix_GetLastSessionPath(char* pathOut, int maxLen) {
    SafeCopyString(g_lastSessionPath, pathOut, maxLen);
}

CAPTURE_API int CamMatrix_GetLastSessionImageCount() {
    return g_lastImageCount.load();
}
