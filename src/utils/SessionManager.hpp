#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <map>

// Simple JSON-like structure instead of nlohmann/json
struct SimpleJSON {
    std::map<std::string, std::string> data;
    void set(const std::string& key, const std::string& value) { data[key] = value; }
    void set(const std::string& key, int value) { data[key] = std::to_string(value); }
    void set(const std::string& key, float value) { data[key] = std::to_string(value); }
    void set(const std::string& key, bool value) { data[key] = value ? "true" : "false"; }
    
    std::string get(const std::string& key, const std::string& default_val = "") const {
        auto it = data.find(key);
        return it != data.end() ? it->second : default_val;
    }
    int getInt(const std::string& key, int default_val = 0) const {
        auto it = data.find(key);
        return it != data.end() ? std::stoi(it->second) : default_val;
    }
    float getFloat(const std::string& key, float default_val = 0.0f) const {
        auto it = data.find(key);
        return it != data.end() ? std::stof(it->second) : default_val;
    }
    bool getBool(const std::string& key, bool default_val = false) const {
        auto it = data.find(key);
        return it != data.end() ? (it->second == "true") : default_val;
    }
};

struct CaptureSession {
    std::string object_name;
    std::string session_id;
    std::string base_path;
    int capture_count;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_capture;
    std::vector<std::string> capture_paths;
    
    CaptureSession() : capture_count(0), created_at(std::chrono::system_clock::now()) {}
    
    std::string GetNextCapturePath() const;
    std::string GetSessionInfo() const;
    SimpleJSON ToJson() const;
    void FromJson(const SimpleJSON& json);
};

class SessionManager {
private:
    std::unique_ptr<CaptureSession> current_session;
    std::string sessions_config_path;
    std::string base_output_path;
    std::vector<CaptureSession> session_history;
    
    void SaveSessionHistory();
    void LoadSessionHistory();
    std::string GenerateSessionId() const;
    
public:
    explicit SessionManager(const std::string& output_path);
    ~SessionManager();
    
    // Session Management
    bool StartNewSession(const std::string& object_name);
    void EndCurrentSession();
    bool HasActiveSession() const;
    CaptureSession* GetCurrentSession();
    
    // Capture Management
    bool RecordCapture(const std::string& capture_path);
    std::string GetNextCapturePath() const;
    int GetTotalCapturesInSession() const;
    
    // History and Statistics
    const std::vector<CaptureSession>& GetSessionHistory() const;
    int GetTotalSessions() const;
    
    // Utility
    void SetOutputPath(const std::string& path);
    std::string GetOutputPath() const;
}; 