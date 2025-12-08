using System;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace CameraMatrixCapture.Services;

/// <summary>
/// P/Invoke wrapper for CaptureCore.dll backend
/// </summary>
public static class CaptureService
{
    private const string DllName = "CaptureCore.dll";

    #region Callback Delegates

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void LogCallbackDelegate(string message);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void ProgressCallbackDelegate(int current, int total);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void DeviceDiscoveredCallbackDelegate(string deviceId, string deviceName);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void CaptureCompleteCallbackDelegate(int success, string sessionPath);

    #endregion

    #region Native Imports - Lifecycle

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_Initialize();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_Shutdown();

    #endregion

    #region Native Imports - Camera Operations

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_DiscoverCameras();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_ConnectAllCameras();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_DisconnectAllCameras();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int CamMatrix_GetDiscoveredCameraCount();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int CamMatrix_GetConnectedCameraCount();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int CamMatrix_IsDiscovering();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int CamMatrix_IsConnecting();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int CamMatrix_IsCapturing();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_GetCameraName(int index, StringBuilder nameOut, int maxLen);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_GetCameraSerial(int index, StringBuilder serialOut, int maxLen);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int CamMatrix_IsCameraConnected(int index);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_SetCameraOrder(int fromIndex, int toIndex);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_ApplySavedCameraOrder();

    #endregion

    #region Native Imports - Capture Operations

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern void CamMatrix_StartCapture(string sessionName, int totalPositions, float angleStep);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_StopCapture();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int CamMatrix_GetCaptureProgress();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int CamMatrix_GetTotalPositions();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern void CamMatrix_CreateSession(string sessionName);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_CaptureOnce();

    #endregion

    #region Native Imports - Bluetooth

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_StartBluetoothScan();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_StopBluetoothScan();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int CamMatrix_IsBluetoothScanning();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int CamMatrix_GetBluetoothDeviceCount();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_GetBluetoothDevice(int index, StringBuilder idOut, int idMaxLen, StringBuilder nameOut, int nameMaxLen);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int CamMatrix_ConnectBluetooth(string deviceId);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_DisconnectBluetooth();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int CamMatrix_IsBluetoothConnected();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int CamMatrix_RotateTurntable(float angle);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int CamMatrix_ReturnToZero();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern float CamMatrix_GetCurrentAngle();

    #endregion

    #region Native Imports - Settings

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_LoadSettings();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_SaveSettings();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int CamMatrix_GetExposureTime();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_SetExposureTime(int microseconds);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern float CamMatrix_GetGain();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_SetGain(float db);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int CamMatrix_GetParallelGroups();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_SetParallelGroups(int groups);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int CamMatrix_GetStaggerDelay();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_SetStaggerDelay(int milliseconds);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_GetOutputPath(StringBuilder pathOut, int maxLen);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern void CamMatrix_SetOutputPath(string path);

    #endregion

    #region Native Imports - Callbacks

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_SetLogCallback(LogCallbackDelegate callback);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_SetProgressCallback(ProgressCallbackDelegate callback);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_SetDeviceDiscoveredCallback(DeviceDiscoveredCallbackDelegate callback);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_SetCaptureCompleteCallback(CaptureCompleteCallbackDelegate callback);

    #endregion

    #region Native Imports - Session Info

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void CamMatrix_GetLastSessionPath(StringBuilder pathOut, int maxLen);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int CamMatrix_GetLastSessionImageCount();

    #endregion

    #region Events

    public static event Action<string>? OnLog;
    public static event Action<int, int>? OnProgress;
    public static event Action<string, string>? OnDeviceDiscovered;
    public static event Action<bool, string>? OnCaptureComplete;

    // Keep delegates alive to prevent GC
    private static LogCallbackDelegate? _logCallback;
    private static ProgressCallbackDelegate? _progressCallback;
    private static DeviceDiscoveredCallbackDelegate? _deviceDiscoveredCallback;
    private static CaptureCompleteCallbackDelegate? _captureCompleteCallback;

    #endregion

    #region Public API - Lifecycle

    public static void Initialize()
    {
        CamMatrix_Initialize();

        // Set up callbacks
        _logCallback = (msg) => OnLog?.Invoke(msg);
        _progressCallback = (current, total) => OnProgress?.Invoke(current, total);
        _deviceDiscoveredCallback = (id, name) => OnDeviceDiscovered?.Invoke(id, name);
        _captureCompleteCallback = (success, path) => OnCaptureComplete?.Invoke(success != 0, path);

        CamMatrix_SetLogCallback(_logCallback);
        CamMatrix_SetProgressCallback(_progressCallback);
        CamMatrix_SetDeviceDiscoveredCallback(_deviceDiscoveredCallback);
        CamMatrix_SetCaptureCompleteCallback(_captureCompleteCallback);
    }

    public static void Shutdown()
    {
        CamMatrix_Shutdown();
    }

    #endregion

    #region Public API - Camera

    public static Task DiscoverCamerasAsync() => Task.Run(CamMatrix_DiscoverCameras);
    public static Task ConnectAllCamerasAsync() => Task.Run(CamMatrix_ConnectAllCameras);
    public static void DisconnectAllCameras() => CamMatrix_DisconnectAllCameras();

    public static int DiscoveredCameraCount => CamMatrix_GetDiscoveredCameraCount();
    public static int ConnectedCameraCount => CamMatrix_GetConnectedCameraCount();

    public static bool IsDiscovering => CamMatrix_IsDiscovering() != 0;
    public static bool IsConnecting => CamMatrix_IsConnecting() != 0;
    public static bool IsCapturing => CamMatrix_IsCapturing() != 0;

    public static string GetCameraName(int index)
    {
        var sb = new StringBuilder(256);
        CamMatrix_GetCameraName(index, sb, sb.Capacity);
        return sb.ToString();
    }

    public static string GetCameraSerial(int index)
    {
        var sb = new StringBuilder(256);
        CamMatrix_GetCameraSerial(index, sb, sb.Capacity);
        return sb.ToString();
    }

    public static bool IsCameraConnected(int index) => CamMatrix_IsCameraConnected(index) != 0;

    public static void SetCameraOrder(int fromIndex, int toIndex) => CamMatrix_SetCameraOrder(fromIndex, toIndex);

    public static void ApplySavedCameraOrder() => CamMatrix_ApplySavedCameraOrder();

    #endregion

    #region Public API - Capture

    public static void StartCapture(string sessionName, int totalPositions, float angleStep)
    {
        CamMatrix_StartCapture(sessionName, totalPositions, angleStep);
    }

    public static void StopCapture() => CamMatrix_StopCapture();

    public static int CaptureProgress => CamMatrix_GetCaptureProgress();
    public static int TotalPositions => CamMatrix_GetTotalPositions();

    /// <summary>
    /// Create a new session directory for manual capture mode
    /// </summary>
    public static void CreateSession(string sessionName)
    {
        CamMatrix_CreateSession(sessionName ?? "");
    }

    /// <summary>
    /// Capture a single frame from all connected cameras (for manual mode)
    /// </summary>
    public static void CaptureOnce(Action? onComplete = null)
    {
        Task.Run(() =>
        {
            CamMatrix_CaptureOnce();
            onComplete?.Invoke();
        });
    }

    #endregion

    #region Public API - Bluetooth

    public static void StartBluetoothScan() => CamMatrix_StartBluetoothScan();
    public static void StopBluetoothScan() => CamMatrix_StopBluetoothScan();
    public static bool IsBluetoothScanning => CamMatrix_IsBluetoothScanning() != 0;

    public static int BluetoothDeviceCount => CamMatrix_GetBluetoothDeviceCount();

    public static (string id, string name) GetBluetoothDevice(int index)
    {
        var idSb = new StringBuilder(256);
        var nameSb = new StringBuilder(256);
        CamMatrix_GetBluetoothDevice(index, idSb, idSb.Capacity, nameSb, nameSb.Capacity);
        return (idSb.ToString(), nameSb.ToString());
    }

    public static bool ConnectBluetooth(string deviceId) => CamMatrix_ConnectBluetooth(deviceId) != 0;
    public static void DisconnectBluetooth() => CamMatrix_DisconnectBluetooth();
    public static bool IsBluetoothConnected => CamMatrix_IsBluetoothConnected() != 0;

    public static bool RotateTurntable(float angle) => CamMatrix_RotateTurntable(angle) != 0;
    public static bool ReturnToZero() => CamMatrix_ReturnToZero() != 0;
    public static float CurrentAngle => CamMatrix_GetCurrentAngle();

    #endregion

    #region Public API - Settings

    public static void LoadSettings() => CamMatrix_LoadSettings();
    public static void SaveSettings() => CamMatrix_SaveSettings();

    public static int ExposureTime
    {
        get => CamMatrix_GetExposureTime();
        set => CamMatrix_SetExposureTime(value);
    }

    public static float Gain
    {
        get => CamMatrix_GetGain();
        set => CamMatrix_SetGain(value);
    }

    public static int ParallelGroups
    {
        get => CamMatrix_GetParallelGroups();
        set => CamMatrix_SetParallelGroups(value);
    }

    public static int StaggerDelay
    {
        get => CamMatrix_GetStaggerDelay();
        set => CamMatrix_SetStaggerDelay(value);
    }

    public static string OutputPath
    {
        get
        {
            var sb = new StringBuilder(512);
            CamMatrix_GetOutputPath(sb, sb.Capacity);
            return sb.ToString();
        }
        set => CamMatrix_SetOutputPath(value);
    }

    #endregion

    #region Public API - Session Info

    public static string LastSessionPath
    {
        get
        {
            var sb = new StringBuilder(512);
            CamMatrix_GetLastSessionPath(sb, sb.Capacity);
            return sb.ToString();
        }
    }

    public static int LastSessionImageCount => CamMatrix_GetLastSessionImageCount();

    #endregion
}
