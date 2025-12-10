/**
 * CaptureAPI.h
 *
 * C-style interface for Camera Matrix Capture backend.
 * This header exposes the core functionality as a DLL for use by WinUI frontend.
 */

#ifndef CAPTURE_API_H
#define CAPTURE_API_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CAPTURECORE_EXPORTS
    #define CAPTURE_API __declspec(dllexport)
#else
    #define CAPTURE_API __declspec(dllimport)
#endif

// ============================================================================
// Callback Types
// ============================================================================

typedef void (*LogCallback)(const char* message);
typedef void (*ProgressCallback)(int current, int total);
typedef void (*DeviceDiscoveredCallback)(const char* deviceId, const char* deviceName);
typedef void (*CaptureCompleteCallback)(int success, const char* sessionPath);

// ============================================================================
// Lifecycle
// ============================================================================

CAPTURE_API void CamMatrix_Initialize();
CAPTURE_API void CamMatrix_Shutdown();

// ============================================================================
// Camera Operations
// ============================================================================

CAPTURE_API void CamMatrix_DiscoverCameras();
CAPTURE_API void CamMatrix_ConnectAllCameras();
CAPTURE_API void CamMatrix_DisconnectAllCameras();

CAPTURE_API int  CamMatrix_GetDiscoveredCameraCount();
CAPTURE_API int  CamMatrix_GetConnectedCameraCount();

CAPTURE_API int  CamMatrix_IsDiscovering();
CAPTURE_API int  CamMatrix_IsConnecting();
CAPTURE_API int  CamMatrix_IsCapturing();

// Get camera info by index (0-based)
CAPTURE_API void CamMatrix_GetCameraName(int index, char* nameOut, int maxLen);
CAPTURE_API void CamMatrix_GetCameraSerial(int index, char* serialOut, int maxLen);
CAPTURE_API int  CamMatrix_IsCameraConnected(int index);

// Camera ordering (for user-defined display order)
CAPTURE_API void CamMatrix_SetCameraOrder(int fromIndex, int toIndex);
CAPTURE_API void CamMatrix_ApplySavedCameraOrder();

// Camera enable/disable for selective capture
CAPTURE_API void CamMatrix_SetCameraEnabled(int index, int enabled);
CAPTURE_API int  CamMatrix_IsCameraEnabled(int index);
CAPTURE_API void CamMatrix_EnableAllCameras();
CAPTURE_API int  CamMatrix_GetEnabledCameraCount();

// ============================================================================
// Capture Operations
// ============================================================================

CAPTURE_API void CamMatrix_StartCapture(const char* sessionName, int totalPositions, float angleStep, float turntableSpeed);
CAPTURE_API void CamMatrix_StopCapture();
CAPTURE_API void CamMatrix_CreateSession(const char* sessionName);
CAPTURE_API void CamMatrix_CaptureOnce();

CAPTURE_API int  CamMatrix_GetCaptureProgress();
CAPTURE_API int  CamMatrix_GetTotalPositions();

// Capture state for timing (0=idle, 1=capturing, 2=rotating, 3=settling)
CAPTURE_API int  CamMatrix_GetCaptureState();
CAPTURE_API int  CamMatrix_GetCaptureElapsedMs();   // Time spent in current capture phase
CAPTURE_API int  CamMatrix_GetRotateElapsedMs();    // Time spent in current rotation phase
CAPTURE_API int  CamMatrix_GetTotalCaptureTimeMs(); // Cumulative capture time
CAPTURE_API int  CamMatrix_GetTotalRotateTimeMs();  // Cumulative rotation time

// ============================================================================
// Bluetooth / Turntable Operations
// ============================================================================

CAPTURE_API void CamMatrix_StartBluetoothScan();
CAPTURE_API void CamMatrix_StopBluetoothScan();
CAPTURE_API int  CamMatrix_IsBluetoothScanning();

CAPTURE_API int  CamMatrix_GetBluetoothDeviceCount();
CAPTURE_API void CamMatrix_GetBluetoothDevice(int index, char* idOut, int idMaxLen, char* nameOut, int nameMaxLen);

CAPTURE_API int  CamMatrix_ConnectBluetooth(const char* deviceId);
CAPTURE_API void CamMatrix_DisconnectBluetooth();
CAPTURE_API int  CamMatrix_IsBluetoothConnected();

CAPTURE_API int  CamMatrix_RotateTurntable(float angle);
CAPTURE_API int  CamMatrix_TiltTurntable(float angle);
CAPTURE_API int  CamMatrix_ReturnToZero();
CAPTURE_API int  CamMatrix_StopTurntable();

CAPTURE_API float CamMatrix_GetCurrentAngle();
CAPTURE_API float CamMatrix_GetCurrentTilt();

// ============================================================================
// Settings Operations
// ============================================================================

CAPTURE_API void CamMatrix_LoadSettings();
CAPTURE_API void CamMatrix_SaveSettings();

// Camera settings
CAPTURE_API int   CamMatrix_GetExposureTime();
CAPTURE_API void  CamMatrix_SetExposureTime(int microseconds);

CAPTURE_API float CamMatrix_GetGain();
CAPTURE_API void  CamMatrix_SetGain(float db);

CAPTURE_API float CamMatrix_GetWhiteBalanceRed();
CAPTURE_API void  CamMatrix_SetWhiteBalanceRed(float value);

CAPTURE_API float CamMatrix_GetWhiteBalanceGreen();
CAPTURE_API void  CamMatrix_SetWhiteBalanceGreen(float value);

CAPTURE_API float CamMatrix_GetWhiteBalanceBlue();
CAPTURE_API void  CamMatrix_SetWhiteBalanceBlue(float value);

// Capture settings
CAPTURE_API int  CamMatrix_GetParallelGroups();
CAPTURE_API void CamMatrix_SetParallelGroups(int groups);

CAPTURE_API int  CamMatrix_GetStaggerDelay();
CAPTURE_API void CamMatrix_SetStaggerDelay(int milliseconds);

CAPTURE_API int  CamMatrix_GetCaptureFormatRaw();
CAPTURE_API void CamMatrix_SetCaptureFormatRaw(int useRaw);

// Output settings
CAPTURE_API void CamMatrix_GetOutputPath(char* pathOut, int maxLen);
CAPTURE_API void CamMatrix_SetOutputPath(const char* path);

// Capture setup settings (persisted)
CAPTURE_API int   CamMatrix_GetCaptureTotalPositions();
CAPTURE_API void  CamMatrix_SetCaptureTotalPositions(int positions);

CAPTURE_API float CamMatrix_GetCaptureAngleStep();
CAPTURE_API void  CamMatrix_SetCaptureAngleStep(float angle);

CAPTURE_API float CamMatrix_GetCaptureTotalRotation();
CAPTURE_API void  CamMatrix_SetCaptureTotalRotation(float rotation);

CAPTURE_API float CamMatrix_GetCaptureTurntableSpeed();
CAPTURE_API void  CamMatrix_SetCaptureTurntableSpeed(float speed);

CAPTURE_API int   CamMatrix_GetCaptureManualMode();
CAPTURE_API void  CamMatrix_SetCaptureManualMode(int isManual);

CAPTURE_API int   CamMatrix_GetCapturePreset();
CAPTURE_API void  CamMatrix_SetCapturePreset(int preset);

// ============================================================================
// Callbacks
// ============================================================================

CAPTURE_API void CamMatrix_SetLogCallback(LogCallback callback);
CAPTURE_API void CamMatrix_SetProgressCallback(ProgressCallback callback);
CAPTURE_API void CamMatrix_SetDeviceDiscoveredCallback(DeviceDiscoveredCallback callback);
CAPTURE_API void CamMatrix_SetCaptureCompleteCallback(CaptureCompleteCallback callback);

// ============================================================================
// Session Info
// ============================================================================

CAPTURE_API void CamMatrix_GetLastSessionPath(char* pathOut, int maxLen);
CAPTURE_API int  CamMatrix_GetLastSessionImageCount();

// ============================================================================
// Debug Logging
// ============================================================================

CAPTURE_API void CamMatrix_GetDebugLogs(char* logsOut, int maxLen);
CAPTURE_API void CamMatrix_ClearDebugLogs();

// ============================================================================
// Working Directory
// ============================================================================

CAPTURE_API void CamMatrix_GetWorkingDirectory(char* pathOut, int maxLen);

#ifdef __cplusplus
}
#endif

#endif // CAPTURE_API_H
