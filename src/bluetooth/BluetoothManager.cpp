#include "BluetoothManager.hpp"
#include "BluetoothDevice.hpp"
#include "BluetoothScanner.hpp"
#include "BluetoothCommands.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <future>
#include <chrono>
#include <thread>

namespace SaperaCapturePro {

BluetoothManager::BluetoothManager() 
    : m_scanner(std::make_unique<BluetoothScanner>()) {
}

BluetoothManager::~BluetoothManager() {
    Shutdown();
}

BluetoothManager& BluetoothManager::GetInstance() {
    static BluetoothManager instance;
    return instance;
}

bool BluetoothManager::Initialize() {
    if (m_initialized) {
        return true;
    }
    
    Log("Initializing Bluetooth Manager...");
    
    // Initialize scanner
    if (!m_scanner->Initialize()) {
        Log("Failed to initialize Bluetooth scanner");
        return false;
    }
    
    // Set scanner callbacks
    m_scanner->SetDeviceDiscoveredCallback(
        [this](const std::string& id, const std::string& name) {
            OnDeviceDiscovered(id, name);
        }
    );
    
    // Load saved settings
    LoadSettings();
    
    m_initialized = true;
    Log("Bluetooth Manager initialized successfully");
    
    return true;
}

void BluetoothManager::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    Log("Shutting down Bluetooth Manager...");
    
    // Stop scanning
    StopScanning();
    
    // Disconnect all devices
    DisconnectAllDevices();
    
    // Save settings
    SaveSettings();
    
    // Shutdown scanner
    if (m_scanner) {
        m_scanner->Shutdown();
    }
    
    m_initialized = false;
    Log("Bluetooth Manager shut down");
}

void BluetoothManager::StartScanning() {
    if (!m_initialized) {
        Log("Bluetooth Manager not initialized");
        return;
    }
    
    if (m_isScanning) {
        Log("Already scanning for devices");
        return;
    }
    
    Log("Starting device scan...");
    
    // Clear old discovered devices
    ClearDiscoveredDevices();
    
    // Start scanner
    if (m_scanner) {
        m_scanner->StartScanning();
        // Check if scanner actually started
        if (m_scanner->IsScanning()) {
            m_isScanning = true;
            Log("Device scan started successfully - click Stop Scan to stop manually");
        } else {
            m_isScanning = false;
            Log("ERROR: Scanner failed to start!");
        }
    } else {
        Log("ERROR: Scanner is null!");
    }
}

void BluetoothManager::StopScanning() {
    if (!m_isScanning) {
        return;
    }
    
    Log("Stopping device scan...");
    
    // Stop scanner
    m_scanner->StopScanning();
    m_isScanning = false;
    
    Log("Device scan stopped");
}

std::vector<std::pair<std::string, std::string>> BluetoothManager::GetDiscoveredDevices() const {
    std::lock_guard<std::mutex> lock(m_discoveredDevicesMutex);
    std::vector<std::pair<std::string, std::string>> devices;
    
    for (const auto& [id, info] : m_discoveredDevices) {
        devices.push_back({id, info.name});
    }
    
    return devices;
}

void BluetoothManager::ClearDiscoveredDevices() {
    std::lock_guard<std::mutex> lock(m_discoveredDevicesMutex);
    m_discoveredDevices.clear();
}

bool BluetoothManager::ConnectToDevice(const std::string& deviceId) {
    if (!m_initialized) {
        Log("Bluetooth Manager not initialized");
        return false;
    }
    
    // Check if already connected
    if (IsDeviceConnected(deviceId)) {
        Log("Device already connected: " + deviceId);
        return true;
    }
    
    Log("Attempting to connect to device ID: " + deviceId);
    
    // Create device instance
    auto device = std::make_unique<BluetoothDevice>(deviceId);
    
    // Set UUIDs
    device->SetServiceUUID(m_serviceUUID);
    device->SetCharacteristicUUID(m_characteristicUUID);
    Log("Using Service UUID: " + m_serviceUUID);
    Log("Using Characteristic UUID: " + m_characteristicUUID);
    
    // Connect
    if (!device->Connect()) {
        Log("Failed to connect to device: " + deviceId);
        Log("Make sure the device is powered on and in range");
        return false;
    }
    
    // Store device
    m_connectedDevices[deviceId] = std::move(device);
    
    // Notify callback
    OnConnectionStatusChanged(deviceId, true);
    
    Log("Successfully connected to device: " + deviceId);
    return true;
}

void BluetoothManager::DisconnectDevice(const std::string& deviceId) {
    auto it = m_connectedDevices.find(deviceId);
    if (it == m_connectedDevices.end()) {
        return;
    }
    
    Log("Disconnecting device: " + deviceId);
    
    // Disconnect
    it->second->Disconnect();
    
    // Remove from map
    m_connectedDevices.erase(it);
    
    // Notify callback
    OnConnectionStatusChanged(deviceId, false);
    
    Log("Disconnected device: " + deviceId);
}

void BluetoothManager::DisconnectAllDevices() {
    std::vector<std::string> deviceIds;
    for (const auto& [id, device] : m_connectedDevices) {
        deviceIds.push_back(id);
    }
    
    for (const auto& id : deviceIds) {
        DisconnectDevice(id);
    }
}

bool BluetoothManager::IsDeviceConnected(const std::string& deviceId) const {
    auto it = m_connectedDevices.find(deviceId);
    if (it == m_connectedDevices.end()) {
        return false;
    }
    return it->second->IsConnected();
}

std::vector<std::string> BluetoothManager::GetConnectedDevices() const {
    std::vector<std::string> devices;
    for (const auto& [id, device] : m_connectedDevices) {
        if (device->IsConnected()) {
            devices.push_back(id);
        }
    }
    return devices;
}

bool BluetoothManager::SendCommand(const std::string& deviceId, const std::string& command) {
    auto it = m_connectedDevices.find(deviceId);
    if (it == m_connectedDevices.end() || !it->second->IsConnected()) {
        Log("Device not connected: " + deviceId);
        return false;
    }
    
    std::string formattedCommand = FormatCommand(command);
    Log("Sending command to " + deviceId + ": " + formattedCommand);
    
    return it->second->SendCommand(formattedCommand);
}

bool BluetoothManager::SendCommandWithResponse(const std::string& deviceId, const std::string& command, CommandResponseCallback callback) {
    auto it = m_connectedDevices.find(deviceId);
    if (it == m_connectedDevices.end() || !it->second->IsConnected()) {
        Log("Device not connected: " + deviceId);
        return false;
    }
    
    std::string formattedCommand = FormatCommand(command);
    Log("Sending command with response to " + deviceId + ": " + formattedCommand);
    
    return it->second->SendCommandWithResponse(formattedCommand, callback);
}

bool BluetoothManager::RotateTurntable(const std::string& deviceId, float angle) {
    std::stringstream cmd;
    cmd << "+CT,TURNANGLE=" << angle << ";";
    return SendCommand(deviceId, cmd.str());
}

float BluetoothManager::GetCurrentAngle(const std::string& deviceId) {
    auto it = m_connectedDevices.find(deviceId);
    if (it == m_connectedDevices.end()) {
        Log("GetCurrentAngle: Device not found: " + deviceId);
        return -1.0f;
    }

    // Use promise/future to make async call synchronous
    std::promise<float> angle_promise;
    auto angle_future = angle_promise.get_future();

    bool sent = SendCommandWithResponse(deviceId, "+QT,CHANGEANGLE;",
        [&angle_promise](const std::string& response) {
            // Parse response like "+DATA=123.45;"
            float angle = -1.0f;
            size_t dataPos = response.find("+DATA=");
            if (dataPos != std::string::npos) {
                size_t startPos = dataPos + 6;  // After "+DATA="
                size_t endPos = response.find(';', startPos);
                if (endPos != std::string::npos) {
                    try {
                        angle = std::stof(response.substr(startPos, endPos - startPos));
                        // Normalize to 0-360
                        while (angle < 0) angle += 360.0f;
                        while (angle >= 360.0f) angle -= 360.0f;
                    } catch (...) {
                        angle = -1.0f;
                    }
                }
            }
            angle_promise.set_value(angle);
        });

    if (!sent) {
        return -1.0f;
    }

    // Wait for response with timeout
    auto status = angle_future.wait_for(std::chrono::milliseconds(2000));
    if (status == std::future_status::ready) {
        return angle_future.get();
    }

    Log("GetCurrentAngle: Response timeout");
    return -1.0f;
}

bool BluetoothManager::RotateTurntableAndWait(const std::string& deviceId, float angle, float turntableSpeed, int timeout_ms) {
    // Calculate expected rotation time based on turntable speed
    // turntableSpeed = seconds for 360° rotation
    // expectedTimeMs = turntableSpeed * 1000 * (angle / 360)
    float expectedTimeMs = turntableSpeed * 1000.0f * (std::abs(angle) / 360.0f);

    Log("RotateTurntableAndWait: Rotating " + std::to_string(angle) + "° at speed " +
        std::to_string(turntableSpeed) + "s/360° (expected: " + std::to_string(static_cast<int>(expectedTimeMs)) + "ms)");

    // Send rotation command
    if (!RotateTurntable(deviceId, angle)) {
        Log("RotateTurntableAndWait: Failed to send rotation command");
        return false;
    }

    auto start_time = std::chrono::steady_clock::now();

    // Wait for 85% of expected time without polling (saves resources)
    int initialWaitMs = static_cast<int>(expectedTimeMs * 0.85f);
    if (initialWaitMs > 50) {
        Log("RotateTurntableAndWait: Waiting " + std::to_string(initialWaitMs) + "ms before verification...");
        std::this_thread::sleep_for(std::chrono::milliseconds(initialWaitMs));
    }

    // Quick stability verification - poll to confirm rotation completed
    const int poll_interval_ms = 30;
    const int stable_required = 2;  // Just 2 stable readings (60ms)
    int stable_count = 0;
    float previous_angle = -999.0f;
    int poll_count = 0;
    const int max_polls = 40;  // Max ~1.2 seconds of polling after initial wait

    while (poll_count < max_polls) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();

        if (elapsed > timeout_ms) {
            Log("RotateTurntableAndWait: TIMEOUT after " + std::to_string(elapsed) + "ms");
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));

        float current_angle = GetCurrentAngle(deviceId);
        if (current_angle < 0) {
            poll_count++;
            continue;
        }

        // Check if angle is stable (not moving)
        if (previous_angle >= 0) {
            float movement = std::abs(current_angle - previous_angle);
            if (movement > 180.0f) movement = 360.0f - movement;

            if (movement < 0.5f) {
                stable_count++;
                if (stable_count >= stable_required) {
                    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start_time).count();
                    Log("RotateTurntableAndWait: Rotation complete at " + std::to_string(current_angle) +
                        "° (total: " + std::to_string(total_time) + "ms, polls: " + std::to_string(poll_count) + ")");
                    return true;
                }
            } else {
                stable_count = 0;  // Reset if still moving
            }
        }

        previous_angle = current_angle;
        poll_count++;
    }

    // If we get here, turntable might still be moving or we timed out on polls
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count();
    Log("RotateTurntableAndWait: Completed after " + std::to_string(total_time) + "ms (max polls reached)");
    return true;  // Consider it done after waiting expected time + polling
}

bool BluetoothManager::SetRotationSpeed(const std::string& deviceId, float speed) {
    std::stringstream cmd;
    cmd << "+CT,TURNSPEED=" << speed << ";";
    return SendCommand(deviceId, cmd.str());
}

bool BluetoothManager::StopRotation(const std::string& deviceId) {
    return SendCommand(deviceId, "+CT,STOP;");
}

bool BluetoothManager::TiltTurntable(const std::string& deviceId, float angle) {
    std::stringstream cmd;
    cmd << "+CR,TILTVALUE=" << angle << ";";
    return SendCommand(deviceId, cmd.str());
}

bool BluetoothManager::SetTiltSpeed(const std::string& deviceId, float speed) {
    std::stringstream cmd;
    cmd << "+CR,TILTSPEED=" << speed << ";";
    return SendCommand(deviceId, cmd.str());
}

bool BluetoothManager::StopTilt(const std::string& deviceId) {
    return SendCommand(deviceId, "+CR,STOP;");
}

bool BluetoothManager::ReturnToZero(const std::string& deviceId) {
    return SendCommand(deviceId, "+CT,TOZERO;");
}

bool BluetoothManager::GetTurntableStatus(const std::string& deviceId, CommandResponseCallback callback) {
    return SendCommandWithResponse(deviceId, "+QT,CHANGEANGLE;", callback);
}

void BluetoothManager::SetServiceUUID(const std::string& uuid) {
    m_serviceUUID = uuid;
    // Update all connected devices
    for (auto& [id, device] : m_connectedDevices) {
        device->SetServiceUUID(uuid);
    }
}

void BluetoothManager::SetCharacteristicUUID(const std::string& uuid) {
    m_characteristicUUID = uuid;
    // Update all connected devices
    for (auto& [id, device] : m_connectedDevices) {
        device->SetCharacteristicUUID(uuid);
    }
}

void BluetoothManager::SaveSettings() {
    try {
        std::filesystem::path settingsPath = "bluetooth_settings.ini";
        std::ofstream file(settingsPath);
        
        if (!file.is_open()) {
            Log("Failed to save Bluetooth settings");
            return;
        }
        
        file << "[Bluetooth]\n";
        file << "ServiceUUID=" << m_serviceUUID << "\n";
        file << "CharacteristicUUID=" << m_characteristicUUID << "\n";
        
        // Save paired devices
        file << "\n[PairedDevices]\n";
        int deviceCount = 0;
        for (const auto& [id, device] : m_connectedDevices) {
            file << "Device" << deviceCount << "=" << id << "\n";
            deviceCount++;
        }
        
        file.close();
        Log("Bluetooth settings saved");
    }
    catch (const std::exception& e) {
        Log("Error saving Bluetooth settings: " + std::string(e.what()));
    }
}

void BluetoothManager::LoadSettings() {
    try {
        std::filesystem::path settingsPath = "bluetooth_settings.ini";
        
        if (!std::filesystem::exists(settingsPath)) {
            Log("No Bluetooth settings file found, using defaults");
            return;
        }
        
        std::ifstream file(settingsPath);
        if (!file.is_open()) {
            Log("Failed to load Bluetooth settings");
            return;
        }
        
        std::string line;
        std::string section;
        
        while (std::getline(file, line)) {
            // Skip empty lines and comments
            if (line.empty() || line[0] == ';' || line[0] == '#') {
                continue;
            }
            
            // Check for section
            if (line[0] == '[' && line.back() == ']') {
                section = line.substr(1, line.length() - 2);
                continue;
            }
            
            // Parse key=value
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                
                if (section == "Bluetooth") {
                    if (key == "ServiceUUID") {
                        m_serviceUUID = value;
                    }
                    else if (key == "CharacteristicUUID") {
                        m_characteristicUUID = value;
                    }
                }
                // Note: Paired devices will be reconnected on demand
            }
        }
        
        file.close();
        Log("Bluetooth settings loaded");
    }
    catch (const std::exception& e) {
        Log("Error loading Bluetooth settings: " + std::string(e.what()));
    }
}

BluetoothManager::DeviceInfo BluetoothManager::GetDeviceInfo(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(m_discoveredDevicesMutex);
    
    auto it = m_discoveredDevices.find(deviceId);
    if (it != m_discoveredDevices.end()) {
        return it->second;
    }
    
    // Check connected devices
    auto connIt = m_connectedDevices.find(deviceId);
    if (connIt != m_connectedDevices.end()) {
        DeviceInfo info;
        info.id = deviceId;
        info.name = connIt->second->GetDeviceName();
        info.connected = connIt->second->IsConnected();
        info.lastSeen = std::chrono::steady_clock::now();
        info.signalStrength = -50; // Default signal strength
        return info;
    }
    
    return DeviceInfo{};
}

void BluetoothManager::Log(const std::string& message) {
    std::string fullMsg = "[Bluetooth] " + message;

    // Always output to debug window
    OutputDebugStringA((fullMsg + "\n").c_str());

    if (m_logCallback) {
        m_logCallback(fullMsg);
    }
}

// Global log function for BluetoothDevice to use
void BluetoothLog(const std::string& message) {
    BluetoothManager::GetInstance().Log(message);
}

void BluetoothManager::OnDeviceDiscovered(const std::string& deviceId, const std::string& deviceName) {
    {
        std::lock_guard<std::mutex> lock(m_discoveredDevicesMutex);
        
        DeviceInfo info;
        info.id = deviceId;
        info.name = deviceName;
        info.connected = false;
        info.lastSeen = std::chrono::steady_clock::now();
        info.signalStrength = -50; // Default signal strength
        
        m_discoveredDevices[deviceId] = info;
    }
    
    Log("Discovered device: " + deviceName + " (" + deviceId + ")");
    
    if (m_deviceDiscoveredCallback) {
        m_deviceDiscoveredCallback(deviceId, deviceName);
    }
}

void BluetoothManager::OnConnectionStatusChanged(const std::string& deviceId, bool connected) {
    {
        std::lock_guard<std::mutex> lock(m_discoveredDevicesMutex);
        
        auto it = m_discoveredDevices.find(deviceId);
        if (it != m_discoveredDevices.end()) {
            it->second.connected = connected;
        }
    }
    
    if (m_connectionStatusCallback) {
        m_connectionStatusCallback(deviceId, connected);
    }
}

std::string BluetoothManager::FormatCommand(const std::string& command) {
    // Ensure command ends with semicolon if not already present
    if (!command.empty() && command.back() != ';') {
        return command + ";";
    }
    return command;
}

// Scan timeout functionality removed for manual control

} // namespace SaperaCapturePro