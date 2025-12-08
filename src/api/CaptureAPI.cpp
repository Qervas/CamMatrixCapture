/**
 * CaptureAPI.cpp
 *
 * Implementation of C-style interface for Camera Matrix Capture backend.
 * Wraps C++ singleton managers for use by WinUI frontend via P/Invoke.
 */

#include "CaptureAPI.h"
#include "CaptureStateMachine.hpp"
#include "../hardware/CameraManager.hpp"
#include "../bluetooth/BluetoothManager.hpp"
#include "../utils/SettingsManager.hpp"
#include "../utils/SessionManager.hpp"

#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <vector>
#include <filesystem>

// Windows API for getting module path
#ifdef _WIN32
#include <Windows.h>
#endif

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

// State machine for capture-rotation cycle
static CaptureStateMachine g_stateMachine;

// Capture state
static std::atomic<int> g_captureProgress{0};
static std::atomic<int> g_totalPositions{0};
static std::string g_lastSessionPath;
static std::atomic<int> g_lastImageCount{0};
static std::atomic<bool> g_isCapturing{false};  // API-level capture state

// Phase timing (for live elapsed time display)
static std::chrono::steady_clock::time_point g_capturePhaseStart;
static std::chrono::steady_clock::time_point g_rotatePhaseStart;

// Turntable state
static std::string g_connectedTurntableId;
static std::atomic<float> g_currentAngle{0.0f};
static std::atomic<float> g_currentTilt{0.0f};

// Debug log buffer
static std::string g_debugLogs;
static std::mutex g_logMutex;

// Working directory (set once at init, used by C# for path resolution)
static std::string g_workingDirectory;

// ============================================================================
// Helper Functions
// ============================================================================

static void SafeLog(const std::string& message) {
    // Add to debug log buffer
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        char timestamp[32];
        std::strftime(timestamp, sizeof(timestamp), "%H:%M:%S", std::localtime(&time_t));
        g_debugLogs += "[" + std::string(timestamp) + "] " + message + "\n";

        // Keep log size reasonable (max 64KB)
        if (g_debugLogs.size() > 65536) {
            g_debugLogs = g_debugLogs.substr(g_debugLogs.size() - 32768);
        }
    }

    // Also output to debug window
    OutputDebugStringA(("[CamMatrix] " + message + "\n").c_str());

    // Call user callback if set
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

    // Set working directory to executable's directory
    // This ensures relative paths (like neural_dataset) are created next to the exe
#ifdef _WIN32
    char modulePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, modulePath, MAX_PATH) > 0) {
        std::filesystem::path exePath(modulePath);
        std::filesystem::path exeDir = exePath.parent_path();
        std::filesystem::current_path(exeDir);
        g_workingDirectory = exeDir.string();
        SafeLog("Working directory set to: " + g_workingDirectory);
    }
#endif

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

// Helper function to apply saved camera order after discovery
static void ApplySavedCameraOrder() {
    if (!g_settingsManager) return;

    auto& orderSettings = g_settingsManager->GetCameraOrderSettings();
    if (!orderSettings.use_custom_ordering || orderSettings.order_entries.empty()) {
        SafeLog("No saved camera order to apply");
        return;
    }

    auto& camMgr = CameraManager::GetInstance();
    auto cameras = camMgr.GetDiscoveredCameras();  // Copy to work with

    if (cameras.empty()) return;

    // Create a map of serial -> desired position
    std::map<std::string, int> serialToPosition;
    for (const auto& entry : orderSettings.order_entries) {
        serialToPosition[entry.serial_number] = entry.display_position;
    }

    // Sort cameras by their saved position (cameras not in saved order go to end)
    std::vector<std::pair<int, int>> sortOrder;  // (saved_position, current_index)
    for (size_t i = 0; i < cameras.size(); ++i) {
        auto it = serialToPosition.find(cameras[i].serialNumber);
        int pos = (it != serialToPosition.end()) ? it->second : 9999 + static_cast<int>(i);
        sortOrder.push_back({pos, static_cast<int>(i)});
    }
    std::sort(sortOrder.begin(), sortOrder.end());

    // Apply the sorted order by moving cameras one at a time
    for (size_t targetPos = 0; targetPos < sortOrder.size(); ++targetPos) {
        int currentPos = sortOrder[targetPos].second;
        if (currentPos != static_cast<int>(targetPos)) {
            // Find where the camera we want is now
            for (size_t j = targetPos; j < sortOrder.size(); ++j) {
                if (sortOrder[j].second == sortOrder[targetPos].second) {
                    // Move it to target position
                    camMgr.ReorderCamera(static_cast<int>(j), static_cast<int>(targetPos));
                    // Update indices for remaining items
                    for (size_t k = targetPos + 1; k < sortOrder.size(); ++k) {
                        if (sortOrder[k].second >= static_cast<int>(targetPos) && sortOrder[k].second < static_cast<int>(j)) {
                            sortOrder[k].second++;
                        }
                    }
                    sortOrder[targetPos].second = static_cast<int>(targetPos);
                    break;
                }
            }
        }
    }

    SafeLog("Applied saved camera order for " + std::to_string(cameras.size()) + " cameras");
}

CAPTURE_API void CamMatrix_DiscoverCameras() {
    CameraManager::GetInstance().DiscoverCameras([](const std::string& msg) {
        SafeLog(msg);
    });

    // Wait for discovery to complete, then apply saved order
    // Note: Discovery runs in background thread, we need to apply order after it completes
    // This is handled by polling IsDiscovering from the frontend
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
    return g_isCapturing ? 1 : 0;
}

CAPTURE_API void CamMatrix_GetCameraName(int index, char* nameOut, int maxLen) {
    auto& cameras = CameraManager::GetInstance().GetDiscoveredCameras();
    if (index >= 0 && index < static_cast<int>(cameras.size())) {
        SafeCopyString(cameras[index].name, nameOut, maxLen);
    } else if (nameOut && maxLen > 0) {
        nameOut[0] = '\0';
    }
}

CAPTURE_API void CamMatrix_GetCameraSerial(int index, char* serialOut, int maxLen) {
    auto& cameras = CameraManager::GetInstance().GetDiscoveredCameras();
    if (index >= 0 && index < static_cast<int>(cameras.size())) {
        SafeCopyString(cameras[index].serialNumber, serialOut, maxLen);
    } else if (serialOut && maxLen > 0) {
        serialOut[0] = '\0';
    }
}

CAPTURE_API int CamMatrix_IsCameraConnected(int index) {
    auto& cameras = CameraManager::GetInstance().GetDiscoveredCameras();
    if (index >= 0 && index < static_cast<int>(cameras.size())) {
        return CameraManager::GetInstance().IsConnected(cameras[index].id) ? 1 : 0;
    }
    return 0;
}

CAPTURE_API void CamMatrix_SetCameraOrder(int fromIndex, int toIndex) {
    CameraManager::GetInstance().ReorderCamera(fromIndex, toIndex);

    // Save the new order to settings
    if (g_settingsManager) {
        auto& orderSettings = g_settingsManager->GetCameraOrderSettings();
        orderSettings.use_custom_ordering = true;
        orderSettings.order_entries.clear();

        // Save current order by serial number
        auto& cameras = CameraManager::GetInstance().GetDiscoveredCameras();
        for (size_t i = 0; i < cameras.size(); ++i) {
            CameraOrderEntry entry;
            entry.serial_number = cameras[i].serialNumber;
            entry.display_position = static_cast<int>(i);
            orderSettings.order_entries.push_back(entry);
        }

        g_settingsManager->Save();
        SafeLog("Camera order saved to settings");
    }

    SafeLog("Camera reordered: " + std::to_string(fromIndex) + " -> " + std::to_string(toIndex));
}

CAPTURE_API void CamMatrix_ApplySavedCameraOrder() {
    ApplySavedCameraOrder();
}

// ============================================================================
// Capture Operations
// ============================================================================

CAPTURE_API void CamMatrix_StartCapture(const char* sessionName, int totalPositions, float angleStep) {
    if (!g_sessionManager) return;
    if (g_isCapturing) return;  // Already capturing

    g_totalPositions = totalPositions;
    g_captureProgress = 0;
    g_isCapturing = true;

    SafeLog("StartCapture: positions=" + std::to_string(totalPositions) + " angleStep=" + std::to_string(angleStep));

    // Ensure session manager uses the current output path from settings
    if (g_settingsManager) {
        auto& appSettings = g_settingsManager->GetAppSettings();
        g_sessionManager->SetOutputPath(appSettings.last_output_folder);
        SafeLog("Output path: " + appSettings.last_output_folder);
    }

    // Create session
    std::string name = sessionName ? sessionName : "";
    g_sessionManager->StartNewSession(name);
    if (g_sessionManager->GetCurrentSession()) {
        g_lastSessionPath = g_sessionManager->GetCurrentSession()->base_path;
        SafeLog("Session path: " + g_lastSessionPath);
    }

    // Get capture params from settings
    CameraManager::CaptureParams params;
    if (g_settingsManager) {
        auto& camSettings = g_settingsManager->GetCameraSettings();
        params.parallel_groups = camSettings.parallel_capture_groups;
        params.group_delay_ms = camSettings.capture_delay_ms;
        params.stagger_delay_ms = camSettings.stagger_delay_ms;
    }

    // Initialize state machine for this capture session
    g_stateMachine.Reset();
    g_stateMachine.ResetTimers();
    g_stateMachine.SetTotalPositions(totalPositions);
    g_stateMachine.SetLogCallback([](const std::string& msg) { SafeLog(msg); });

    // Start capture thread with turntable rotation
    std::thread captureThread([params, angleStep]() {
        auto& camMgr = CameraManager::GetInstance();
        auto& btMgr = BluetoothManager::GetInstance();

        SafeLog("Capture thread started, totalPositions=" + std::to_string(g_totalPositions.load()));

        for (int pos = 0; pos < g_totalPositions.load(); ++pos) {
            // Check stop flag
            if (!g_isCapturing) {
                SafeLog("Capture stopped by user at position " + std::to_string(pos));
                g_stateMachine.ProcessEvent(CaptureEvent::Stop);
                break;
            }

            g_stateMachine.SetCurrentPosition(pos + 1);
            SafeLog("=== Position " + std::to_string(pos + 1) + "/" + std::to_string(g_totalPositions.load()) + " ===");

            // Create position folder
            std::string posPath = g_lastSessionPath + "\\pos_" + std::to_string(pos + 1);
            std::filesystem::create_directories(posPath);

            // ==================== CAPTURE PHASE ====================
            if (!g_stateMachine.ProcessEvent(CaptureEvent::StartCapture)) {
                SafeLog("[ERROR] Invalid state transition to Capturing!");
                g_stateMachine.ProcessEvent(CaptureEvent::Reset);
                g_stateMachine.ProcessEvent(CaptureEvent::StartCapture);
            }

            g_capturePhaseStart = std::chrono::steady_clock::now();
            g_stateMachine.StartPhaseTimer();

            bool captureSuccess = camMgr.CaptureAllCameras(posPath, params);

            // Record capture time
            auto captureEnd = std::chrono::steady_clock::now();
            int captureMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(captureEnd - g_capturePhaseStart).count());
            g_stateMachine.RecordCaptureTime(captureMs);

            SafeLog("[CAPTURE] " + std::string(captureSuccess ? "Complete" : "Warning: errors") + " (" + std::to_string(captureMs) + "ms)");

            g_captureProgress = pos + 1;
            if (g_progressCallback) {
                g_progressCallback(pos + 1, g_totalPositions.load());
            }

            // ==================== ROTATION PHASE ====================
            bool needsRotation = (pos < g_totalPositions.load() - 1 && !g_connectedTurntableId.empty());

            if (needsRotation) {
                // Direct transition: Capturing -> Rotating
                g_stateMachine.ProcessEvent(CaptureEvent::StartRotation);

                g_rotatePhaseStart = std::chrono::steady_clock::now();
                g_stateMachine.StartPhaseTimer();

                SafeLog("[ROTATE] Rotating " + std::to_string(angleStep) + "°...");

                // Simple delta rotation - just rotate by the step angle
                bool rotateSuccess = btMgr.RotateTurntableAndWait(g_connectedTurntableId, angleStep, 30000);

                // Record rotation time
                auto rotateEnd = std::chrono::steady_clock::now();
                int rotateMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(rotateEnd - g_rotatePhaseStart).count());
                g_stateMachine.RecordRotationTime(rotateMs);

                // Update displayed angle (for UI only)
                g_currentAngle = g_currentAngle.load() + angleStep;
                while (g_currentAngle >= 360.0f) g_currentAngle = g_currentAngle.load() - 360.0f;

                SafeLog("[ROTATE] Complete (" + std::to_string(rotateMs) + "ms)");

                if (rotateSuccess) {
                    g_stateMachine.ProcessEvent(CaptureEvent::RotationComplete);
                } else {
                    SafeLog("[ROTATE] Warning: Rotation may not have completed properly");
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                    g_stateMachine.ProcessEvent(CaptureEvent::RotationFailed);
                }

                // ==================== SETTLING PHASE ====================
                SafeLog("[SETTLE] Waiting 300ms for turntable to settle...");
                std::this_thread::sleep_for(std::chrono::milliseconds(300));

                g_stateMachine.ProcessEvent(CaptureEvent::SettlingComplete);
            } else {
                // No rotation needed (last position)
                g_stateMachine.ProcessEvent(CaptureEvent::CaptureComplete);
            }

            // Brief pause before next capture cycle
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // Capture complete
        g_lastImageCount = g_captureProgress.load() * camMgr.GetConnectedCount();
        g_isCapturing = false;
        g_stateMachine.Reset();

        SafeLog("Capture sequence completed: " + std::to_string(g_captureProgress.load()) + " positions, " +
                std::to_string(g_lastImageCount.load()) + " images");
        SafeLog("Total capture time: " + std::to_string(g_stateMachine.GetTotalCaptureTimeMs()) + "ms, " +
                "Total rotation time: " + std::to_string(g_stateMachine.GetTotalRotationTimeMs()) + "ms");

        if (g_captureCompleteCallback) {
            g_captureCompleteCallback(1, g_lastSessionPath.c_str());
        }
    });
    captureThread.detach();
}

CAPTURE_API void CamMatrix_StopCapture() {
    SafeLog("StopCapture called");
    g_isCapturing = false;
    g_stateMachine.ProcessEvent(CaptureEvent::Stop);
}

CAPTURE_API void CamMatrix_CreateSession(const char* sessionName) {
    if (!g_sessionManager) {
        SafeLog("CreateSession failed: session manager not initialized");
        return;
    }

    std::string name = sessionName ? sessionName : "";
    g_sessionManager->StartNewSession(name);

    if (g_sessionManager->GetCurrentSession()) {
        g_lastSessionPath = g_sessionManager->GetCurrentSession()->base_path;
        g_captureProgress = 0;
        SafeLog("Session created: " + g_lastSessionPath);
    }
}

CAPTURE_API void CamMatrix_CaptureOnce() {
    if (g_lastSessionPath.empty()) {
        SafeLog("CaptureOnce failed: no active session. Call CreateSession first.");
        return;
    }

    SafeLog("CaptureOnce: capturing to " + g_lastSessionPath);

    CameraManager::CaptureParams params;
    if (g_settingsManager) {
        auto& camSettings = g_settingsManager->GetCameraSettings();
        params.parallel_groups = camSettings.parallel_capture_groups;
        params.group_delay_ms = camSettings.capture_delay_ms;
        params.stagger_delay_ms = camSettings.stagger_delay_ms;
    }

    // Create a subfolder for this capture (pos_N)
    g_captureProgress = g_captureProgress.load() + 1;
    std::string capturePath = g_lastSessionPath + "/pos_" + std::to_string(g_captureProgress.load());
    std::filesystem::create_directories(capturePath);

    CameraManager::GetInstance().CaptureAllCameras(capturePath, params);

    // Update last image count
    g_lastImageCount = g_captureProgress.load() * CameraManager::GetInstance().GetConnectedCount();

    SafeLog("CaptureOnce completed: " + std::to_string(g_captureProgress.load()) + " captures");
}

CAPTURE_API int CamMatrix_GetCaptureProgress() {
    return g_captureProgress.load();
}

CAPTURE_API int CamMatrix_GetTotalPositions() {
    return g_totalPositions.load();
}

CAPTURE_API int CamMatrix_GetCaptureState() {
    // Get state from state machine (returns int: 0=idle, 1=capturing, 2=rotating, 3=settling, 4=error)
    return g_stateMachine.GetStateInt();
}

CAPTURE_API int CamMatrix_GetCaptureElapsedMs() {
    // If currently capturing, calculate elapsed time
    if (g_stateMachine.IsInState(CaptureState::Capturing)) {
        auto now = std::chrono::steady_clock::now();
        return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now - g_capturePhaseStart).count());
    }
    return g_stateMachine.GetCurrentCaptureElapsedMs();
}

CAPTURE_API int CamMatrix_GetRotateElapsedMs() {
    // If currently rotating, calculate elapsed time
    if (g_stateMachine.IsInState(CaptureState::Rotating)) {
        auto now = std::chrono::steady_clock::now();
        return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now - g_rotatePhaseStart).count());
    }
    return g_stateMachine.GetCurrentRotationElapsedMs();
}

CAPTURE_API int CamMatrix_GetTotalCaptureTimeMs() {
    // If currently capturing, add current elapsed to total
    if (g_stateMachine.IsInState(CaptureState::Capturing)) {
        auto now = std::chrono::steady_clock::now();
        int currentElapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now - g_capturePhaseStart).count());
        return g_stateMachine.GetTotalCaptureTimeMs() + currentElapsed;
    }
    return g_stateMachine.GetTotalCaptureTimeMs();
}

CAPTURE_API int CamMatrix_GetTotalRotateTimeMs() {
    // If currently rotating, add current elapsed to total
    if (g_stateMachine.IsInState(CaptureState::Rotating)) {
        auto now = std::chrono::steady_clock::now();
        int currentElapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now - g_rotatePhaseStart).count());
        return g_stateMachine.GetTotalRotationTimeMs() + currentElapsed;
    }
    return g_stateMachine.GetTotalRotationTimeMs();
}

// ============================================================================
// Bluetooth / Turntable Operations
// ============================================================================

CAPTURE_API void CamMatrix_StartBluetoothScan() {
    auto& btMgr = BluetoothManager::GetInstance();

    // Initialize if not already done
    if (!btMgr.IsScanning()) {
        btMgr.Initialize();
    }

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

    SafeLog("ConnectBluetooth called with deviceId: " + std::string(deviceId));

    // Ensure initialized
    BluetoothManager::GetInstance().Initialize();

    bool success = BluetoothManager::GetInstance().ConnectToDevice(deviceId);
    SafeLog("ConnectBluetooth: ConnectToDevice returned " + std::to_string(success));
    if (success) {
        g_connectedTurntableId = deviceId;
        SafeLog("ConnectBluetooth: g_connectedTurntableId set to '" + g_connectedTurntableId + "'");
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
    SafeLog("RotateTurntable called with angle=" + std::to_string(angle) + ", turntableId='" + g_connectedTurntableId + "'");
    if (g_connectedTurntableId.empty()) {
        SafeLog("RotateTurntable: No turntable connected!");
        return 0;
    }
    bool success = BluetoothManager::GetInstance().RotateTurntable(g_connectedTurntableId, angle);
    SafeLog("RotateTurntable: success=" + std::to_string(success));
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
        pathOut[0] = '\0';  // No default - user must select
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

// ============================================================================
// Debug Logging
// ============================================================================

CAPTURE_API void CamMatrix_GetDebugLogs(char* logsOut, int maxLen) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    SafeCopyString(g_debugLogs, logsOut, maxLen);
}

CAPTURE_API void CamMatrix_ClearDebugLogs() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_debugLogs.clear();
}

// ============================================================================
// Working Directory
// ============================================================================

CAPTURE_API void CamMatrix_GetWorkingDirectory(char* pathOut, int maxLen) {
    SafeCopyString(g_workingDirectory, pathOut, maxLen);
}
