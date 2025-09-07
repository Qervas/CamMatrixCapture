#pragma once

#include <windows.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Storage.Streams.h>

#include <string>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>

namespace SaperaCapturePro {

using CommandResponseCallback = std::function<void(const std::string& response)>;

class BluetoothDevice {
public:
    BluetoothDevice(const std::string& deviceId);
    ~BluetoothDevice();
    
    // Connection management
    bool Connect();
    void Disconnect();
    bool IsConnected() const { return m_connected; }
    
    // Device information
    std::string GetDeviceId() const { return m_deviceId; }
    std::string GetDeviceName() const { return m_deviceName; }
    
    // Command sending
    bool SendCommand(const std::string& command);
    bool SendCommandWithResponse(const std::string& command, CommandResponseCallback callback);
    
    // Configuration
    void SetServiceUUID(const std::string& uuid) { m_serviceUUID = uuid; }
    void SetCharacteristicUUID(const std::string& uuid) { m_characteristicUUID = uuid; }
    
    // Async operations
    winrt::Windows::Foundation::IAsyncOperation<bool> ConnectAsync();
    winrt::Windows::Foundation::IAsyncOperation<bool> SendCommandAsync(const std::string& command);
    winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> ReadResponseAsync();
    
private:
    // WinRT device objects
    winrt::Windows::Devices::Bluetooth::BluetoothLEDevice m_device{ nullptr };
    winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattDeviceService m_service{ nullptr };
    winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic m_characteristic{ nullptr };
    
    // Device information
    std::string m_deviceId;
    std::string m_deviceName;
    
    // Configuration
    std::string m_serviceUUID = "0000ffe0-0000-1000-8000-00805f9b34fb";
    std::string m_characteristicUUID = "0000ffe1-0000-1000-8000-00805f9b34fb";
    
    // State
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_connecting{false};
    
    // Command queue for sequential operations
    struct Command {
        std::string data;
        CommandResponseCallback responseCallback;
        bool expectsResponse;
    };
    
    std::queue<Command> m_commandQueue;
    std::mutex m_commandQueueMutex;
    std::thread m_commandProcessorThread;
    std::atomic<bool> m_processingCommands{false};
    
    // Internal methods
    void ProcessCommandQueue();
    winrt::guid StringToGuid(const std::string& str);
};

} // namespace SaperaCapturePro