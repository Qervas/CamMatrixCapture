#include "BluetoothDevice.hpp"
#include <iostream>
#include <sstream>
#include <chrono>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Storage::Streams;

namespace SaperaCapturePro {

BluetoothDevice::BluetoothDevice(const std::string& deviceId) 
    : m_deviceId(deviceId) {
    // Don't initialize apartment here - it should be done once at application startup
}

BluetoothDevice::~BluetoothDevice() {
    Disconnect();
}

bool BluetoothDevice::Connect() {
    if (m_connected) {
        return true;
    }
    
    if (m_connecting) {
        return false;
    }
    
    m_connecting = true;
    
    try {
        // Run async connection in blocking manner
        bool result = ConnectAsync().get();
        m_connecting = false;
        
        if (result && m_connected) {
            // Start command processor thread
            m_processingCommands = true;
            m_commandProcessorThread = std::thread(&BluetoothDevice::ProcessCommandQueue, this);
        }
        
        return result;
    }
    catch (...) {
        m_connecting = false;
        return false;
    }
}

void BluetoothDevice::Disconnect() {
    if (!m_connected) {
        return;
    }
    
    m_connected = false;
    
    // Stop command processor
    m_processingCommands = false;
    if (m_commandProcessorThread.joinable()) {
        m_commandProcessorThread.join();
    }
    
    // Close device
    if (m_device) {
        m_device.Close();
        m_device = nullptr;
    }
    
    m_service = nullptr;
    m_characteristic = nullptr;
    
    // Clear command queue
    std::lock_guard<std::mutex> lock(m_commandQueueMutex);
    while (!m_commandQueue.empty()) {
        m_commandQueue.pop();
    }
}

bool BluetoothDevice::SendCommand(const std::string& command) {
    if (!m_connected) {
        return false;
    }
    
    Command cmd;
    cmd.data = command;
    cmd.expectsResponse = false;
    
    {
        std::lock_guard<std::mutex> lock(m_commandQueueMutex);
        m_commandQueue.push(cmd);
    }
    
    return true;
}

bool BluetoothDevice::SendCommandWithResponse(const std::string& command, CommandResponseCallback callback) {
    if (!m_connected) {
        return false;
    }
    
    Command cmd;
    cmd.data = command;
    cmd.responseCallback = callback;
    cmd.expectsResponse = true;
    
    {
        std::lock_guard<std::mutex> lock(m_commandQueueMutex);
        m_commandQueue.push(cmd);
    }
    
    return true;
}

IAsyncOperation<bool> BluetoothDevice::ConnectAsync() {
    try {
        // Use device ID directly with FromIdAsync (like in the working example)
        std::wstring wideDeviceId(m_deviceId.begin(), m_deviceId.end());
        m_device = co_await BluetoothLEDevice::FromIdAsync(wideDeviceId);
        
        if (!m_device) {
            co_return false;
        }
        
        // Store device name
        auto name = m_device.Name();
        m_deviceName = winrt::to_string(name);
        
        // Get GATT services - try with UUID first, then get all services if that fails
        auto serviceGuid = StringToGuid(m_serviceUUID);
        auto servicesResult = co_await m_device.GetGattServicesForUuidAsync(serviceGuid);
        
        if (servicesResult.Status() != GattCommunicationStatus::Success || 
            servicesResult.Services().Size() == 0) {
            // Try getting all services
            auto allServicesResult = co_await m_device.GetGattServicesAsync();
            if (allServicesResult.Status() == GattCommunicationStatus::Success) {
                // Log available services for debugging
                for (auto service : allServicesResult.Services()) {
                    // Look for the service we want
                    if (service.Uuid() == serviceGuid) {
                        m_service = service;
                        break;
                    }
                }
            }
            
            if (!m_service) {
                co_return false;
            }
        } else {
            m_service = servicesResult.Services().GetAt(0);
        }
        
        // Get characteristic
        auto charGuid = StringToGuid(m_characteristicUUID);
        auto charsResult = co_await m_service.GetCharacteristicsForUuidAsync(charGuid);
        
        if (charsResult.Status() != GattCommunicationStatus::Success || 
            charsResult.Characteristics().Size() == 0) {
            // Try getting all characteristics
            auto allCharsResult = co_await m_service.GetCharacteristicsAsync();
            if (allCharsResult.Status() == GattCommunicationStatus::Success) {
                for (auto characteristic : allCharsResult.Characteristics()) {
                    if (characteristic.Uuid() == charGuid) {
                        m_characteristic = characteristic;
                        break;
                    }
                }
            }
            
            if (!m_characteristic) {
                co_return false;
            }
        } else {
            m_characteristic = charsResult.Characteristics().GetAt(0);
        }
        
        m_connected = true;
        co_return true;
    }
    catch (...) {
        co_return false;
    }
}

IAsyncOperation<bool> BluetoothDevice::SendCommandAsync(const std::string& command) {
    if (!m_connected || !m_characteristic) {
        co_return false;
    }
    
    try {
        // Convert string to bytes
        DataWriter writer;
        std::vector<uint8_t> data(command.begin(), command.end());
        writer.WriteBytes(data);
        
        // Send command
        auto result = co_await m_characteristic.WriteValueWithResultAsync(
            writer.DetachBuffer(), 
            GattWriteOption::WriteWithoutResponse
        );
        
        co_return (result.Status() == GattCommunicationStatus::Success);
    }
    catch (...) {
        co_return false;
    }
}

IAsyncOperation<winrt::hstring> BluetoothDevice::ReadResponseAsync() {
    if (!m_connected || !m_characteristic) {
        co_return winrt::hstring{};
    }
    
    try {
        auto result = co_await m_characteristic.ReadValueAsync();
        if (result.Status() == GattCommunicationStatus::Success) {
            DataReader reader = DataReader::FromBuffer(result.Value());
            std::vector<uint8_t> data(result.Value().Length());
            reader.ReadBytes(data);
            std::string response(data.begin(), data.end());
            std::wstring wresponse(response.begin(), response.end());
            co_return winrt::hstring{ wresponse };
        }
    }
    catch (...) {
        // Ignore read errors
    }
    
    co_return winrt::hstring{};
}

void BluetoothDevice::ProcessCommandQueue() {
    while (m_processingCommands) {
        Command cmd;
        bool hasCommand = false;
        
        {
            std::lock_guard<std::mutex> lock(m_commandQueueMutex);
            if (!m_commandQueue.empty()) {
                cmd = m_commandQueue.front();
                m_commandQueue.pop();
                hasCommand = true;
            }
        }
        
        if (hasCommand) {
            // Send command
            bool sent = SendCommandAsync(cmd.data).get();
            
            if (sent && cmd.expectsResponse && cmd.responseCallback) {
                // Wait a bit for response
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                // Read response
                auto response = ReadResponseAsync().get();
                if (!response.empty()) {
                    cmd.responseCallback(winrt::to_string(response));
                }
            }
        }
        else {
            // No commands, sleep a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

winrt::guid BluetoothDevice::StringToGuid(const std::string& str) {
    // Parse UUID string to GUID
    // Format: "0000ffe0-0000-1000-8000-00805f9b34fb"
    
    std::string cleanStr = str;
    cleanStr.erase(std::remove(cleanStr.begin(), cleanStr.end(), '-'), cleanStr.end());
    
    if (cleanStr.length() != 32) {
        // Return default GUID if invalid
        return winrt::guid{};
    }
    
    // Parse hex string to GUID components
    uint32_t data1;
    uint16_t data2, data3;
    uint8_t data4[8];
    
    std::stringstream ss;
    ss << std::hex;
    
    ss.str(cleanStr.substr(0, 8));
    ss >> data1;
    ss.clear();
    
    ss.str(cleanStr.substr(8, 4));
    ss >> data2;
    ss.clear();
    
    ss.str(cleanStr.substr(12, 4));
    ss >> data3;
    ss.clear();
    
    for (int i = 0; i < 8; i++) {
        ss.str(cleanStr.substr(16 + i * 2, 2));
        int temp;
        ss >> temp;
        data4[i] = static_cast<uint8_t>(temp);
        ss.clear();
    }
    
    return winrt::guid{ data1, data2, data3, 
        { data4[0], data4[1], data4[2], data4[3], 
          data4[4], data4[5], data4[6], data4[7] } };
}

} // namespace SaperaCapturePro