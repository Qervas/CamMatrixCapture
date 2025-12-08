#include "SessionManager.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>

namespace fs = std::filesystem;

// CaptureSession implementation
std::string CaptureSession::GetNextCapturePath() const {
    fs::path p(base_path);
    std::stringstream ss;
    ss << "capture_" << std::setfill('0') << std::setw(3) << (capture_count + 1);
    p /= ss.str();
    return p.string();
}

std::string CaptureSession::GetSessionInfo() const {
    auto created_time = std::chrono::system_clock::to_time_t(created_at);
    std::stringstream info;
    info << object_name << " (" << capture_count << " captures)";
    return info.str();
}

SimpleJSON CaptureSession::ToJson() const {
    SimpleJSON json;
    json.set("object_name", object_name);
    json.set("session_id", session_id);
    json.set("base_path", base_path);
    json.set("capture_count", capture_count);
    
    auto created_time = std::chrono::system_clock::to_time_t(created_at);
    json.set("created_at", std::to_string(created_time));
    
    auto last_time = std::chrono::system_clock::to_time_t(last_capture);
    json.set("last_capture", std::to_string(last_time));
    
    return json;
}

void CaptureSession::FromJson(const SimpleJSON& json) {
    object_name = json.get("object_name");
    session_id = json.get("session_id");
    base_path = json.get("base_path");
    capture_count = json.getInt("capture_count");
    
    auto created_time = json.getInt("created_at");
    created_at = std::chrono::system_clock::from_time_t(created_time);
    
    auto last_time = json.getInt("last_capture");
    last_capture = std::chrono::system_clock::from_time_t(last_time);
}

// SessionManager implementation
SessionManager::SessionManager(const std::string& output_path) 
    : base_output_path(output_path) {
    
    sessions_config_path = output_path + "/sessions.config";
    
    // Create base directory
    try {
        fs::create_directories(base_output_path);
        fs::create_directories(base_output_path + "/images");
        fs::create_directories(base_output_path + "/metadata");
    } catch (const std::exception&) {
        // Directory creation failed, but continue
    }
    
    LoadSessionHistory();
}

SessionManager::~SessionManager() {
    if (current_session) {
        EndCurrentSession();
    }
    SaveSessionHistory();
}

bool SessionManager::StartNewSession(const std::string& object_name) {
    if (current_session) {
        EndCurrentSession();
    }
    
    current_session = std::make_unique<CaptureSession>();
    current_session->object_name = object_name;
    current_session->session_id = GenerateSessionId();
    
    // Create session folder name with timestamp: session_name_YYYYMMDD_HHMMSS
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    std::string timestamp = ss.str();
    
    // Create folder name: session_name_timestamp
    std::string folder_name = object_name + "_" + timestamp;
    
    // Use std::filesystem::path to ensure proper path separators on Windows
    fs::path sessionPath = fs::path(base_output_path) / "images" / folder_name;
    current_session->base_path = sessionPath.string();
    current_session->created_at = std::chrono::system_clock::now();
    
    // Create session directory
    try {
        fs::create_directories(current_session->base_path);
        return true;
    } catch (const std::exception&) {
        current_session.reset();
        return false;
    }
}

void SessionManager::EndCurrentSession() {
    if (current_session) {
        // Add to history
        session_history.push_back(*current_session);
        
        // Save updated history
        SaveSessionHistory();
        
        // Clear current session
        current_session.reset();
    }
}

bool SessionManager::HasActiveSession() const {
    return current_session != nullptr;
}

CaptureSession* SessionManager::GetCurrentSession() {
    return current_session.get();
}

bool SessionManager::RecordCapture(const std::string& capture_path) {
    if (!current_session) {
        return false;
    }
    
    current_session->capture_count++;
    current_session->last_capture = std::chrono::system_clock::now();
    current_session->capture_paths.push_back(capture_path);
    
    return true;
}

std::string SessionManager::GetNextCapturePath() const {
    if (!current_session) {
        return "";
    }
    
    return current_session->GetNextCapturePath();
}

int SessionManager::GetTotalCapturesInSession() const {
    return current_session ? current_session->capture_count : 0;
}

const std::vector<CaptureSession>& SessionManager::GetSessionHistory() const {
    return session_history;
}

int SessionManager::GetTotalSessions() const {
    return static_cast<int>(session_history.size()) + (current_session ? 1 : 0);
}

void SessionManager::SetOutputPath(const std::string& path) {
    base_output_path = path;
    sessions_config_path = path + "/sessions.config";
}

std::string SessionManager::GetOutputPath() const {
    return base_output_path;
}

void SessionManager::SaveSessionHistory() {
    try {
        std::ofstream file(sessions_config_path);
        if (!file.is_open()) {
            return;
        }
        
        file << session_history.size() << std::endl;
        
        for (const auto& session : session_history) {
            auto json = session.ToJson();
            for (const auto& [key, value] : json.data) {
                file << key << "=" << value << std::endl;
            }
            file << "---" << std::endl; // Session separator
        }
    } catch (const std::exception&) {
        // Failed to save, but continue
    }
}

void SessionManager::LoadSessionHistory() {
    try {
        std::ifstream file(sessions_config_path);
        if (!file.is_open()) {
            return;
        }
        
        std::string line;
        if (!std::getline(file, line)) {
            return;
        }
        
        int session_count = std::stoi(line);
        session_history.clear();
        session_history.reserve(session_count);
        
        for (int i = 0; i < session_count; ++i) {
            CaptureSession session;
            SimpleJSON json;
            
            while (std::getline(file, line) && line != "---") {
                auto pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string key = line.substr(0, pos);
                    std::string value = line.substr(pos + 1);
                    json.data[key] = value;
                }
            }
            
            session.FromJson(json);
            session_history.push_back(session);
        }
    } catch (const std::exception&) {
        // Failed to load, start with empty history
        session_history.clear();
    }
}

std::string SessionManager::GenerateSessionId() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    
    // Add random suffix
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100, 999);
    ss << "_" << dis(gen);
    
    return ss.str();
} 