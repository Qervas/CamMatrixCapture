#include "BluetoothScanner.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Enumeration;

namespace SaperaCapturePro {

BluetoothScanner::BluetoothScanner() {
    // Don't initialize apartment here - it should be done once at application startup
}

BluetoothScanner::~BluetoothScanner() {
    Shutdown();
}

bool BluetoothScanner::Initialize() {
    // Simplified initialization - just mark as ready
    m_initialized = true;
    return true;
}

void BluetoothScanner::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    // Stop scanning first
    StopScanning();
    
    // Clean up device watcher completely
    if (m_deviceWatcher) {
        try {
            // Make sure it's stopped
            auto status = m_deviceWatcher.Status();
            if (status == DeviceWatcherStatus::Started || 
                status == DeviceWatcherStatus::EnumerationCompleted ||
                status == DeviceWatcherStatus::Stopping) {
                m_deviceWatcher.Stop();
                // Give it time to stop
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }
        catch (...) {
            // Ignore errors during shutdown
        }
        
        // Clear the watcher reference
        m_deviceWatcher = nullptr;
    }
    
    // Clear advertisement watcher if it exists
    if (m_advertisementWatcher) {
        try {
            m_advertisementWatcher.Stop();
        }
        catch (...) {
            // Ignore errors
        }
        m_advertisementWatcher = nullptr;
    }
    
    // Clear discovered devices
    ClearDiscoveredDevices();
    
    m_initialized = false;
}

void BluetoothScanner::StartScanning() {
    if (m_isScanning) {
        OutputDebugStringA("[BLE Scanner] Already scanning\n");
        return;
    }
    
    OutputDebugStringA("[BLE Scanner] Starting scan (using working pattern)...\n");
    ClearDiscoveredDevices();
    
    try {
        // Use the EXACT pattern from working code
        auto deviceSelector = BluetoothLEDevice::GetDeviceSelector();
        m_deviceWatcher = DeviceInformation::CreateWatcher(deviceSelector);
        
        // Set up event handler exactly like working code
        m_deviceAddedToken = m_deviceWatcher.Added([this](DeviceWatcher const& sender, DeviceInformation const& deviceInfo) {
            OnDeviceAdded(sender, deviceInfo);
        });
        
        // Start the watcher
        m_deviceWatcher.Start();
        m_isScanning = true;
        OutputDebugStringA("[BLE Scanner] Device watcher started using working pattern!\n");
    }
    catch (winrt::hresult_error const& ex) {
        std::wstring msg = std::wstring(L"[BLE Scanner] ERROR starting scan: ") + ex.message().c_str() + L"\n";
        OutputDebugStringW(msg.c_str());
        m_isScanning = false;
    }
    catch (...) {
        OutputDebugStringA("[BLE Scanner] ERROR: Unknown exception during scan start\n");
        m_isScanning = false;
    }
}

void BluetoothScanner::StopScanning() {
    if (!m_isScanning) {
        return;
    }
    
    m_isScanning = false; // Set this first to prevent race conditions
    
    try {
        // Stop device watcher safely
        if (m_deviceWatcher) {
            auto status = m_deviceWatcher.Status();
            if (status == DeviceWatcherStatus::Started || 
                status == DeviceWatcherStatus::EnumerationCompleted ||
                status == DeviceWatcherStatus::Stopping) {
                m_deviceWatcher.Stop();
                
                // Wait a moment for the watcher to stop properly
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            // Clear the watcher after stopping to ensure clean state
            m_deviceWatcher = nullptr;
        }
    }
    catch (...) {
        // Handle error gracefully - don't crash the application
    }
}

std::vector<std::pair<std::string, std::string>> BluetoothScanner::GetDiscoveredDevices() const {
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    
    std::vector<std::pair<std::string, std::string>> devices;
    for (const auto& [id, name] : m_discoveredDevices) {
        devices.push_back({id, name});
    }
    
    return devices;
}

void BluetoothScanner::ClearDiscoveredDevices() {
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    m_discoveredDevices.clear();
}

void BluetoothScanner::OnDeviceAdded(DeviceWatcher const& sender, DeviceInformation const& deviceInfo) {
    try {
        auto id = deviceInfo.Id();
        auto name = deviceInfo.Name();
        
        // Accept all devices, even those without names (we can get names later)
        std::string deviceId = winrt::to_string(id);
        std::string deviceName = winrt::to_string(name);
        
        // If no name, create a placeholder name
        if (deviceName.empty()) {
            deviceName = "BLE Device (unnamed)";
        }
        
        {
            std::lock_guard<std::mutex> lock(m_devicesMutex);
            m_discoveredDevices[deviceId] = deviceName;
        }
        
        if (m_deviceDiscoveredCallback) {
            m_deviceDiscoveredCallback(deviceId, deviceName);
        }
    }
    catch (...) {
        // Handle error
    }
}

void BluetoothScanner::OnDeviceUpdated(DeviceWatcher const& sender, DeviceInformationUpdate const& deviceUpdate) {
    // Device update handling if needed
}

void BluetoothScanner::OnAdvertisementReceived(BluetoothLEAdvertisementWatcher const& sender, 
                                              BluetoothLEAdvertisementReceivedEventArgs const& args) {
    try {
        // Get Bluetooth address
        uint64_t address = args.BluetoothAddress();
        
        // Try to get local name from advertisement
        std::string deviceName;
        
        auto advertisement = args.Advertisement();
        if (advertisement) {
            auto localName = advertisement.LocalName();
            if (!localName.empty()) {
                deviceName = winrt::to_string(localName);
            }
        }
        
        // If no name found, try to get it from the device asynchronously
        if (deviceName.empty()) {
            try {
                auto bleDevice = BluetoothLEDevice::FromBluetoothAddressAsync(address).get();
                if (bleDevice) {
                    auto name = bleDevice.Name();
                    if (!name.empty()) {
                        deviceName = winrt::to_string(name);
                    }
                }
            } catch (...) {
                // Ignore errors in getting device name
            }
        }
        
        // Fallback to a generic name with address
        if (deviceName.empty()) {
            std::string addressStr = FormatBluetoothAddress(address);
            deviceName = "BLE Device (" + addressStr + ")";
        }
        
        // Convert address to string for device ID (this is what FromBluetoothAddressAsync expects)
        std::wstringstream wss;
        wss << address;
        std::string deviceId = winrt::to_string(wss.str());
        
        {
            std::lock_guard<std::mutex> lock(m_devicesMutex);
            
            // Store the bluetooth address as the device ID
            if (m_discoveredDevices.find(deviceId) == m_discoveredDevices.end()) {
                m_discoveredDevices[deviceId] = deviceName;
                
                if (m_deviceDiscoveredCallback) {
                    m_deviceDiscoveredCallback(deviceId, deviceName);
                }
            }
        }
    }
    catch (...) {
        // Handle error
    }
}

std::string BluetoothScanner::WideStringToString(const std::wstring& wstr) {
    if (wstr.empty()) {
        return std::string();
    }
    
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string str(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, nullptr, nullptr);
    
    return str;
}

std::string BluetoothScanner::FormatBluetoothAddress(uint64_t address) {
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0');
    
    for (int i = 5; i >= 0; i--) {
        ss << std::setw(2) << ((address >> (i * 8)) & 0xFF);
        if (i > 0) {
            ss << ":";
        }
    }
    
    return ss.str();
}

} // namespace SaperaCapturePro