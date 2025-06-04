#ifndef CAMERA_API_HPP
#define CAMERA_API_HPP

#include <string>
#include <functional>
#include <map>
#include <set>
#include <mutex>
#include "CameraConfigManager.hpp"

// HTTP-like response structure (can be adapted to actual web framework)
struct APIResponse {
    int statusCode = 200;
    std::string contentType = "application/json";
    std::string body;
    
    static APIResponse success(const simple_json::JsonValue& data);
    static APIResponse error(int code, const std::string& message);
};

// Request structure
struct APIRequest {
    std::string method; // GET, POST, PUT, DELETE
    std::string path;
    std::string body;
    std::map<std::string, std::string> params;
    std::map<std::string, std::string> query;
};

class CameraAPI {
public:
    CameraAPI(CameraConfigManager& configManager);
    
    // API Endpoints
    APIResponse handleRequest(const APIRequest& request);
    
    // Camera discovery and status
    APIResponse getCameraList(const APIRequest& request);           // GET /api/cameras
    APIResponse getCameraStatus(const APIRequest& request);         // GET /api/cameras/{serial}
    APIResponse refreshCameras(const APIRequest& request);          // POST /api/cameras/refresh
    
    // Parameter management
    APIResponse getParameters(const APIRequest& request);           // GET /api/cameras/{serial}/parameters
    APIResponse setParameter(const APIRequest& request);            // PUT /api/cameras/{serial}/parameters/{param}
    APIResponse setParameters(const APIRequest& request);           // PUT /api/cameras/{serial}/parameters
    APIResponse resetParameters(const APIRequest& request);         // POST /api/cameras/{serial}/parameters/reset
    
    // Global defaults
    APIResponse getDefaults(const APIRequest& request);             // GET /api/defaults
    APIResponse setDefaults(const APIRequest& request);             // PUT /api/defaults
    
    // Presets
    APIResponse getPresets(const APIRequest& request);              // GET /api/presets
    APIResponse savePreset(const APIRequest& request);              // POST /api/presets/{name}
    APIResponse loadPreset(const APIRequest& request);              // PUT /api/cameras/{serial}/presets/{name}
    APIResponse deletePreset(const APIRequest& request);            // DELETE /api/presets/{name}
    
    // Capture operations
    APIResponse captureImage(const APIRequest& request);            // POST /api/cameras/{serial}/capture
    APIResponse captureAll(const APIRequest& request);              // POST /api/capture
    APIResponse captureBurst(const APIRequest& request);            // POST /api/capture/burst
    
    // Parameter history and monitoring
    APIResponse getParameterHistory(const APIRequest& request);     // GET /api/history
    APIResponse clearParameterHistory(const APIRequest& request);   // DELETE /api/history
    
    // Configuration persistence
    APIResponse saveConfiguration(const APIRequest& request);       // POST /api/config/save
    APIResponse loadConfiguration(const APIRequest& request);       // POST /api/config/load
    APIResponse exportConfiguration(const APIRequest& request);     // GET /api/config/export
    
private:
    CameraConfigManager& configManager_;
    
    // Helper methods
    std::string extractSerialFromPath(const std::string& path);
    std::string extractParameterFromPath(const std::string& path);
    std::string extractPresetFromPath(const std::string& path);
    simple_json::JsonValue parseRequestBody(const std::string& body);
    
    // Route matching
    using RouteHandler = std::function<APIResponse(const APIRequest&)>;
    std::map<std::pair<std::string, std::string>, RouteHandler> routes_; // {method, path} -> handler
    void registerRoutes();
    bool matchRoute(const std::string& pattern, const std::string& path) const;
};

// WebSocket-like interface for real-time updates
class CameraWebSocketHandler {
public:
    CameraWebSocketHandler(CameraConfigManager& configManager);
    
    // Connection management
    void onConnect(const std::string& clientId);
    void onDisconnect(const std::string& clientId);
    void onMessage(const std::string& clientId, const std::string& message);
    
    // Real-time notifications
    void broadcastParameterChange(const std::string& serialNumber, const CameraParameters& params);
    void broadcastCameraStatus(const std::string& serialNumber, bool connected);
    void broadcastCaptureEvent(const std::string& serialNumber, const std::string& filename);
    
private:
    CameraConfigManager& configManager_;
    std::set<std::string> connectedClients_;
    std::mutex clientsMutex_;
    
    void sendToClient(const std::string& clientId, const simple_json::JsonValue& message);
    void broadcastToAll(const simple_json::JsonValue& message);
};

#endif // CAMERA_API_HPP 