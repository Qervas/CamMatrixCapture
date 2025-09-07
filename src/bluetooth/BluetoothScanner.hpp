#pragma once

#include <windows.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <string>
#include <functional>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>

namespace SaperaCapturePro {

using DeviceDiscoveredCallback = std::function<void(const std::string& deviceId, const std::string& deviceName)>;

class BluetoothScanner {
public:
    BluetoothScanner();
    ~BluetoothScanner();
    
    // Initialization
    bool Initialize();
    void Shutdown();
    
    // Scanning control
    void StartScanning();
    void StopScanning();
    bool IsScanning() const { return m_isScanning; }
    
    // Device discovery
    std::vector<std::pair<std::string, std::string>> GetDiscoveredDevices() const;
    void ClearDiscoveredDevices();
    
    // Callbacks
    void SetDeviceDiscoveredCallback(DeviceDiscoveredCallback callback) { 
        m_deviceDiscoveredCallback = callback; 
    }
    
private:
    // WinRT objects
    winrt::Windows::Devices::Enumeration::DeviceWatcher m_deviceWatcher{ nullptr };
    winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher m_advertisementWatcher{ nullptr };
    
    // Event tokens for cleanup
    winrt::event_token m_deviceAddedToken;
    winrt::event_token m_deviceUpdatedToken;
    winrt::event_token m_deviceRemovedToken;
    winrt::event_token m_advertisementReceivedToken;
    
    // Discovered devices
    mutable std::mutex m_devicesMutex;
    std::map<std::string, std::string> m_discoveredDevices; // deviceId -> deviceName
    
    // State
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_isScanning{false};
    
    // Callbacks
    DeviceDiscoveredCallback m_deviceDiscoveredCallback;
    
    // Event handlers
    void OnDeviceAdded(winrt::Windows::Devices::Enumeration::DeviceWatcher const& sender,
                       winrt::Windows::Devices::Enumeration::DeviceInformation const& deviceInfo);
    
    void OnDeviceUpdated(winrt::Windows::Devices::Enumeration::DeviceWatcher const& sender,
                        winrt::Windows::Devices::Enumeration::DeviceInformationUpdate const& deviceUpdate);
    
    void OnAdvertisementReceived(winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher const& sender,
                                 winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementReceivedEventArgs const& args);
    
    // Helper methods
    std::string WideStringToString(const std::wstring& wstr);
    std::string FormatBluetoothAddress(uint64_t address);
};

} // namespace SaperaCapturePro