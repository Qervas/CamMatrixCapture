#include "BluetoothDevice.hpp"
#include "BluetoothManager.hpp"
#include <iostream>
#include <sstream>
#include <chrono>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Storage::Streams;

namespace SaperaCapturePro {

// Helper macro for logging
#define BLE_LOG(msg) BluetoothLog(msg)

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
        // Run async connection with timeout to prevent hanging
        auto asyncOp = ConnectAsync();

        // Wait for up to 10 seconds
        const int timeoutMs = 10000;
        const int pollIntervalMs = 100;
        int elapsed = 0;

        while (elapsed < timeoutMs) {
            auto status = asyncOp.Status();
            if (status == winrt::Windows::Foundation::AsyncStatus::Completed) {
                bool result = asyncOp.GetResults();
                m_connecting = false;

                if (result && m_connected) {
                    // Start command processor thread
                    BLE_LOG("Connect: Starting command processor thread");
                    m_processingCommands = true;
                    m_commandProcessorThread = std::thread(&BluetoothDevice::ProcessCommandQueue, this);
                    BLE_LOG("Connect: Command processor thread started");
                } else {
                    BLE_LOG("Connect: result=" + std::to_string(result) + " m_connected=" + std::to_string(m_connected.load()));
                }

                return result;
            }
            else if (status == winrt::Windows::Foundation::AsyncStatus::Error ||
                     status == winrt::Windows::Foundation::AsyncStatus::Canceled) {
                m_connecting = false;
                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
            elapsed += pollIntervalMs;
        }

        // Timeout - cancel the operation
        asyncOp.Cancel();
        m_connecting = false;
        return false;
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
        BLE_LOG("SendCommand: Not connected, command='" + command + "'");
        return false;
    }

    Command cmd;
    cmd.data = command;
    cmd.expectsResponse = false;

    {
        std::lock_guard<std::mutex> lock(m_commandQueueMutex);
        m_commandQueue.push(cmd);
        BLE_LOG("SendCommand: Queued command '" + command + "', queue size=" + std::to_string(m_commandQueue.size()));
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
        BLE_LOG("ConnectAsync started, deviceId: " + m_deviceId);

        // Use device ID directly with FromIdAsync
        std::wstring wideDeviceId(m_deviceId.begin(), m_deviceId.end());
        BLE_LOG("Calling FromIdAsync...");
        m_device = co_await BluetoothLEDevice::FromIdAsync(wideDeviceId);

        if (!m_device) {
            BLE_LOG("ERROR: FromIdAsync returned null device");
            co_return false;
        }

        // Store device name
        auto name = m_device.Name();
        m_deviceName = winrt::to_string(name);
        BLE_LOG("Got device: " + m_deviceName);

        // Discover all services and find a writable characteristic dynamically
        // No hardcoded UUIDs - works with any BLE device
        BLE_LOG("Discovering GATT services...");
        auto allServicesResult = co_await m_device.GetGattServicesAsync(BluetoothCacheMode::Cached);
        if (allServicesResult.Status() != GattCommunicationStatus::Success ||
            allServicesResult.Services().Size() == 0) {
            allServicesResult = co_await m_device.GetGattServicesAsync(BluetoothCacheMode::Uncached);
        }

        if (allServicesResult.Status() != GattCommunicationStatus::Success) {
            BLE_LOG("ERROR: Failed to get GATT services");
            co_return false;
        }

        // Helper lambda to format GUID as string for logging
        auto guidToString = [](const winrt::guid& g) -> std::string {
            char buf[64];
            snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                g.Data1, g.Data2, g.Data3,
                g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3],
                g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
            return std::string(buf);
        };

        // First pass: Look for FFE0/FFE1 (common HM-10 module used by turntables)
        // These are the standard UUIDs for serial communication on cheap BLE modules
        BLE_LOG("Pass 1: Looking for FFE0/FFE1 service (HM-10 module)...");
        for (auto service : allServicesResult.Services()) {
            auto serviceUuid = guidToString(service.Uuid());

            // Check if this is the FFE0 service (case insensitive check for ffe0)
            if (serviceUuid.find("0000ffe0") == 0 || serviceUuid.find("0000FFE0") == 0) {
                BLE_LOG("Found FFE0 service: " + serviceUuid);
                auto charsResult = co_await service.GetCharacteristicsAsync(BluetoothCacheMode::Uncached);
                if (charsResult.Status() == GattCommunicationStatus::Success) {
                    for (auto characteristic : charsResult.Characteristics()) {
                        auto charUuid = guidToString(characteristic.Uuid());
                        BLE_LOG("  Checking characteristic: " + charUuid);

                        // FFE1 is the TX/RX characteristic
                        if (charUuid.find("0000ffe1") == 0 || charUuid.find("0000FFE1") == 0) {
                            m_service = service;
                            m_characteristic = characteristic;
                            m_connected = true;
                            BLE_LOG("SUCCESS: Found FFE1 characteristic, connected!");
                            co_return true;
                        }
                    }
                }
            }
        }

        // Second pass: Find any writable characteristic as fallback
        BLE_LOG("Pass 2: FFE0/FFE1 not found, looking for any writable characteristic...");
        int serviceIdx = 0;
        for (auto service : allServicesResult.Services()) {
            auto serviceUuid = guidToString(service.Uuid());
            BLE_LOG("Checking service " + std::to_string(serviceIdx++) + ": " + serviceUuid);

            auto charsResult = co_await service.GetCharacteristicsAsync(BluetoothCacheMode::Cached);
            if (charsResult.Status() != GattCommunicationStatus::Success ||
                charsResult.Characteristics().Size() == 0) {
                charsResult = co_await service.GetCharacteristicsAsync(BluetoothCacheMode::Uncached);
                if (charsResult.Status() != GattCommunicationStatus::Success) {
                    continue;
                }
            }

            BLE_LOG("Service has " + std::to_string(charsResult.Characteristics().Size()) + " characteristics");

            for (auto characteristic : charsResult.Characteristics()) {
                auto charUuid = guidToString(characteristic.Uuid());
                auto props = characteristic.CharacteristicProperties();

                bool canWrite = (static_cast<uint32_t>(props) &
                    static_cast<uint32_t>(GattCharacteristicProperties::Write)) != 0;
                bool canWriteNoResponse = (static_cast<uint32_t>(props) &
                    static_cast<uint32_t>(GattCharacteristicProperties::WriteWithoutResponse)) != 0;

                BLE_LOG("  Char " + charUuid + " props=" + std::to_string(static_cast<uint32_t>(props)) +
                        " canWrite=" + std::to_string(canWrite) + " canWriteNoResponse=" + std::to_string(canWriteNoResponse));

                if (canWrite || canWriteNoResponse) {
                    m_service = service;
                    m_characteristic = characteristic;
                    m_connected = true;
                    BLE_LOG("SUCCESS: Found writable characteristic, connected!");
                    co_return true;
                }
            }
        }

        BLE_LOG("ERROR: No writable characteristic found in any service");
        co_return false;
    }
    catch (winrt::hresult_error const& ex) {
        BLE_LOG("EXCEPTION: " + winrt::to_string(ex.message()));
        co_return false;
    }
    catch (...) {
        BLE_LOG("EXCEPTION: Unknown error");
        co_return false;
    }
}

IAsyncOperation<bool> BluetoothDevice::SendCommandAsync(const std::string& command) {
    if (!m_connected || !m_characteristic) {
        BLE_LOG("SendCommandAsync: Not connected or no characteristic");
        co_return false;
    }

    try {
        BLE_LOG("SendCommandAsync: Sending '" + command + "'");

        // Convert string to bytes
        DataWriter writer;
        std::vector<uint8_t> data(command.begin(), command.end());
        writer.WriteBytes(data);

        // Send command
        auto result = co_await m_characteristic.WriteValueWithResultAsync(
            writer.DetachBuffer(),
            GattWriteOption::WriteWithoutResponse
        );

        bool success = (result.Status() == GattCommunicationStatus::Success);
        BLE_LOG("SendCommandAsync: Result=" + std::to_string(static_cast<int>(result.Status())) +
                " success=" + std::to_string(success));
        co_return success;
    }
    catch (winrt::hresult_error const& ex) {
        BLE_LOG("SendCommandAsync EXCEPTION: " + winrt::to_string(ex.message()));
        co_return false;
    }
    catch (...) {
        BLE_LOG("SendCommandAsync: Unknown exception");
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
    BLE_LOG("ProcessCommandQueue: Thread started");
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
            BLE_LOG("ProcessCommandQueue: Processing command '" + cmd.data + "'");
            // Send command
            bool sent = SendCommandAsync(cmd.data).get();
            BLE_LOG("ProcessCommandQueue: Command sent=" + std::to_string(sent));

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
    BLE_LOG("ProcessCommandQueue: Thread exiting");
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