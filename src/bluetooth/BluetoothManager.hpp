#pragma once

#include <memory>
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

// Forward declarations to avoid including WinRT headers in the header
namespace winrt::Windows::Devices::Bluetooth {
    class BluetoothLEDevice;
}

namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile {
    class GattDeviceService;
    class GattCharacteristic;
}

namespace SaperaCapturePro {

// Forward declarations
class BluetoothDevice;
class BluetoothScanner;

// Callback types
using DeviceDiscoveredCallback = std::function<void(const std::string& deviceId, const std::string& deviceName)>;
using ConnectionStatusCallback = std::function<void(const std::string& deviceId, bool connected)>;
using CommandResponseCallback = std::function<void(const std::string& response)>;
using LogCallback = std::function<void(const std::string& message)>;

class BluetoothManager {
public:
    // Singleton access
    static BluetoothManager& GetInstance();
    
    // Delete copy and move constructors
    BluetoothManager(const BluetoothManager&) = delete;
    BluetoothManager& operator=(const BluetoothManager&) = delete;
    BluetoothManager(BluetoothManager&&) = delete;
    BluetoothManager& operator=(BluetoothManager&&) = delete;
    
    // Initialization and cleanup
    bool Initialize();
    void Shutdown();
    
    // Device discovery
    void StartScanning();
    void StopScanning();
    bool IsScanning() const { return m_isScanning; }
    std::vector<std::pair<std::string, std::string>> GetDiscoveredDevices() const;
    void ClearDiscoveredDevices();
    
    // Device connection
    bool ConnectToDevice(const std::string& deviceId);
    void DisconnectDevice(const std::string& deviceId);
    void DisconnectAllDevices();
    bool IsDeviceConnected(const std::string& deviceId) const;
    std::vector<std::string> GetConnectedDevices() const;
    
    // Command sending
    bool SendCommand(const std::string& deviceId, const std::string& command);
    bool SendCommandWithResponse(const std::string& deviceId, const std::string& command, CommandResponseCallback callback);
    
    // Turntable-specific commands
    bool RotateTurntable(const std::string& deviceId, float angle);
    bool SetRotationSpeed(const std::string& deviceId, float speed);
    bool StopRotation(const std::string& deviceId);
    bool TiltTurntable(const std::string& deviceId, float angle);
    bool SetTiltSpeed(const std::string& deviceId, float speed);
    bool StopTilt(const std::string& deviceId);
    bool ReturnToZero(const std::string& deviceId);
    bool GetTurntableStatus(const std::string& deviceId, CommandResponseCallback callback);
    
    // Callbacks
    void SetDeviceDiscoveredCallback(DeviceDiscoveredCallback callback) { m_deviceDiscoveredCallback = callback; }
    void SetConnectionStatusCallback(ConnectionStatusCallback callback) { m_connectionStatusCallback = callback; }
    void SetLogCallback(LogCallback callback) { m_logCallback = callback; }
    
    // Configuration
    void SetServiceUUID(const std::string& uuid);
    void SetCharacteristicUUID(const std::string& uuid);
    std::string GetServiceUUID() const { return m_serviceUUID; }
    std::string GetCharacteristicUUID() const { return m_characteristicUUID; }
    
    // Settings persistence
    void SaveSettings();
    void LoadSettings();
    
    // Device information
    struct DeviceInfo {
        std::string id;
        std::string name;
        bool connected;
        std::chrono::steady_clock::time_point lastSeen;
        int signalStrength;
    };
    
    DeviceInfo GetDeviceInfo(const std::string& deviceId) const;
    
private:
    BluetoothManager();
    ~BluetoothManager();
    
    // Scanner instance
    std::unique_ptr<BluetoothScanner> m_scanner;
    
    // Connected devices
    std::map<std::string, std::unique_ptr<BluetoothDevice>> m_connectedDevices;
    
    // Discovered devices
    mutable std::mutex m_discoveredDevicesMutex;
    std::map<std::string, DeviceInfo> m_discoveredDevices;
    
    // State
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_isScanning{false};
    
    // Configuration
    std::string m_serviceUUID = "0000ffe0-0000-1000-8000-00805f9b34fb";
    std::string m_characteristicUUID = "0000ffe1-0000-1000-8000-00805f9b34fb";
    
    // Callbacks
    DeviceDiscoveredCallback m_deviceDiscoveredCallback;
    ConnectionStatusCallback m_connectionStatusCallback;
    LogCallback m_logCallback;
    
    // Internal methods
    void Log(const std::string& message);
    void OnDeviceDiscovered(const std::string& deviceId, const std::string& deviceName);
    void OnConnectionStatusChanged(const std::string& deviceId, bool connected);
    std::string FormatCommand(const std::string& command);
    
    // Scan timeout functionality removed for manual control
};

} // namespace SaperaCapturePro