/**
 * SaperaCapturePro - Professional HTTP Server
 * 
 * Enterprise-grade HTTP server with REST API and embedded web interface
 * Copyright (c) 2024 SaperaCapturePro. All rights reserved.
 */

#pragma once

#include "../core/CaptureEngine.hpp"
#include "../utils/JsonHelper.hpp"
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <map>

namespace SaperaCapturePro {

/**
 * HTTP request structure
 */
struct HttpRequest {
    std::string method;           // GET, POST, PUT, DELETE
    std::string path;             // Request path
    std::string query;            // Query string
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> queryParams;
    std::string body;             // Request body
    std::string clientAddress;   // Client IP address
    
    JsonValue toJson() const;
};

/**
 * HTTP response structure
 */
struct HttpResponse {
    int statusCode = 200;
    std::string statusMessage = "OK";
    std::map<std::string, std::string> headers;
    std::string body;
    std::string contentType = "application/json";
    
    // Convenience constructors
    static HttpResponse success(const JsonValue& data);
    static HttpResponse error(int code, const std::string& message);
    static HttpResponse html(const std::string& content);
    static HttpResponse file(const std::string& filePath, const std::string& contentType);
    
    void setJson(const JsonValue& data);
    void setCors(bool enable = true);
    JsonValue toJson() const;
};

/**
 * Route handler function type
 */
using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;

/**
 * Professional HTTP server with embedded REST API
 */
class HttpServer {
public:
    /**
     * Constructor
     * @param port HTTP server port
     * @param captureEngine Reference to the capture engine
     */
    HttpServer(uint16_t port, CaptureEngine& captureEngine);
    
    /**
     * Destructor
     */
    ~HttpServer();
    
    // Server lifecycle
    bool start();
    void stop();
    bool isRunning() const { return running_; }
    
    // Configuration
    void setDocumentRoot(const std::string& path) { documentRoot_ = path; }
    void enableCors(bool enable = true) { corsEnabled_ = enable; }
    void setMaxRequestSize(size_t maxSize) { maxRequestSize_ = maxSize; }
    
    // Statistics
    struct ServerStatistics {
        uint64_t totalRequests = 0;
        uint64_t successfulRequests = 0;
        uint64_t failedRequests = 0;
        double averageResponseTimeMs = 0.0;
        std::string uptime = "";
        
        JsonValue toJson() const;
    };
    
    ServerStatistics getStatistics() const;
    void resetStatistics();
    
private:
    // Internal server implementation
    class HttpServerImpl;
    std::unique_ptr<HttpServerImpl> impl_;
    
    // Server configuration
    uint16_t port_;
    CaptureEngine& captureEngine_;
    std::string documentRoot_ = "web";
    bool corsEnabled_ = true;
    size_t maxRequestSize_ = 16 * 1024 * 1024; // 16MB
    
    // Server state
    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> serverThread_;
    mutable std::mutex statisticsMutex_;
    ServerStatistics statistics_;
    std::chrono::steady_clock::time_point startTime_;
    
    // Route management
    std::map<std::pair<std::string, std::string>, RouteHandler> routes_;
    RouteHandler defaultHandler_;
    
    // Server implementation
    void serverLoop();
    void registerRoutes();
    HttpResponse handleRequest(const HttpRequest& request);
    HttpResponse routeRequest(const HttpRequest& request);
    void updateStatistics(const HttpRequest& request, const HttpResponse& response, 
                         double responseTimeMs);
    
    // Static file serving
    HttpResponse serveStaticFile(const std::string& path);
    std::string getMimeType(const std::string& extension) const;
    
    // REST API endpoints
    
    // System endpoints
    HttpResponse apiGetStatus(const HttpRequest& request);
    HttpResponse apiGetVersion(const HttpRequest& request);
    HttpResponse apiGetStatistics(const HttpRequest& request);
    
    // Camera endpoints
    HttpResponse apiGetCameras(const HttpRequest& request);
    HttpResponse apiGetCamera(const HttpRequest& request);
    HttpResponse apiRefreshCameras(const HttpRequest& request);
    
    // Parameter endpoints
    HttpResponse apiGetParameters(const HttpRequest& request);
    HttpResponse apiSetParameter(const HttpRequest& request);
    HttpResponse apiSetParameters(const HttpRequest& request);
    
    // Capture endpoints
    HttpResponse apiCaptureCamera(const HttpRequest& request);
    HttpResponse apiCaptureAll(const HttpRequest& request);
    
    // Preset endpoints
    HttpResponse apiGetPresets(const HttpRequest& request);
    HttpResponse apiApplyPreset(const HttpRequest& request);
    HttpResponse apiSavePreset(const HttpRequest& request);
    HttpResponse apiDeletePreset(const HttpRequest& request);
    
    // Configuration endpoints
    HttpResponse apiGetConfiguration(const HttpRequest& request);
    HttpResponse apiSetConfiguration(const HttpRequest& request);
    HttpResponse apiExportConfiguration(const HttpRequest& request);
    
    // Utility methods
    std::string extractPathParameter(const std::string& pattern, 
                                    const std::string& path, 
                                    const std::string& paramName) const;
    std::map<std::string, std::string> parseQueryString(const std::string& query) const;
    bool matchRoute(const std::string& pattern, const std::string& path) const;
};

} // namespace SaperaCapturePro 