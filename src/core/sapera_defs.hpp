#pragma once
#include <string>
#include <vector>
#include <cstring> // For strncpy
#include <atomic>  // For tracking device instances
#include <iostream> // Replace QDebug with iostream

// Using our custom Sapera stubs for consistent behavior
namespace SaperaStubs {
    // Forward declarations
    class SapManager;
    class SapLocation;
    class SapAcqDevice;
    class SapBuffer;
    class SapBufferWithTrash;
    class SapAcqDeviceToBuf;
    class SapTransfer;
    class SapXferCallbackInfo;
    class SapView;
    class SapFeature;
    
    // Define necessary constants
    constexpr int CORSERVER_MAX_STRLEN = 256;
    typedef unsigned int UINT32;
    typedef int BOOL;
    constexpr BOOL TRUE = 1;
    constexpr BOOL FALSE = 0;
    
    // Define buffer format constants
    namespace SapFormatConstants {
        constexpr int SAPBUFFER_FORMAT_MONO8 = 0;
        constexpr int SAPBUFFER_FORMAT_MONO16 = 1;
        constexpr int SAPBUFFER_FORMAT_RGB24 = 2;
        constexpr int SAPBUFFER_FORMAT_RGB32 = 3;
    }
    
    // Callback function declaration - defined before SapXferCallbackInfo
    class SapXferCallbackInfo;
    typedef void (*XferCallbackFunc)(SapXferCallbackInfo*);
    
    // Define SapManVersionInfo structure to match Sapera SDK
    class SapManVersionInfo {
    protected:
        int m_Major;
        int m_Minor;
        int m_Revision;
        int m_Build;
        char m_Date[64];
        char m_Time[64];
        
    public:
        SapManVersionInfo() : m_Major(8), m_Minor(70), m_Revision(0), m_Build(1) {
            strcpy(m_Date, "Apr 15 2025");
            strcpy(m_Time, "12:00:00");
        }
        
        // Add accessor methods
        int GetMajor() const { return m_Major; }
        int GetMinor() const { return m_Minor; }
        int GetRevision() const { return m_Revision; }
        int GetBuild() const { return m_Build; }
    };
    
    // SapManager class
    class SapManager {
    public:
        static int GetServerCount() { 
            // Get the actual number of cameras from the system
            return 64;  // Support up to 64 cameras
        }
        
        static bool GetServerName(int index, char* serverName, size_t maxLen) {
            // Get camera name from the system
            if (index >= 0 && index < GetServerCount()) {
                if (index == 0) {
                    strncpy(serverName, "Nano-C4020_Main", maxLen);
                } else {
                    snprintf(serverName, maxLen, "Nano-C4020_%d", index-1);
                }
                return true;
            }
            return false;
        }
        
        // Match the real Sapera SDK method signature
        static BOOL GetVersionInfo(SapManVersionInfo* pVersionInfo) {
            if (pVersionInfo) {
                return TRUE;
            }
            return FALSE;
        }
    };
    
    // Stub SapLocation class
    class SapLocation {
    public:
        enum LocationType {
            ServerSystem = 0,
            ServerFile = 1,
            ServerDef = 2,
            ServerRemote = 3,
            ServerUnknown = 4
        };
        
        SapLocation() {}
        SapLocation(const char* serverName) {}
    };
    
    // Feature class for camera control
    class SapFeature {
    public:
        enum AccessMode { AccessRO, AccessWO, AccessRW };
        
        SapFeature(const SapLocation& loc) {}
        bool Create() { return true; }
        bool Destroy() { return true; }
        bool GetAccessMode(AccessMode* pMode) { 
            *pMode = AccessRW; 
            return true; 
        }
        bool GetEnumCount(int* pCount) { 
            *pCount = 4; 
            return true; 
        }
        bool GetEnumString(int index, char* enumString, size_t maxLen) {
            const char* formats[] = { "Mono8", "Mono16", "RGB24", "RGB32" };
            if (index >= 0 && index < 4) {
                strncpy(enumString, formats[index], maxLen);
                return true;
            }
            return false;
        }
    };
    
    // Forward declaration for SapXferCallbackInfo which is used in SapAcqDevice
    typedef void (*SaperaXferCallbackType)(SapXferCallbackInfo*);
    
    // Device class
    class SapAcqDevice {
    private:
        // Static counter to track the number of device instances
        // Use inline to avoid multiple definition errors
        inline static std::atomic<int> s_deviceCount{0};
        // No hard limit on devices - system resources are the only constraint
        
        bool m_isConnected;
        std::string m_serverName;
        
    public:
        SapAcqDevice() : m_isConnected(false) {}
        
        SapAcqDevice(const char* serverName) : m_isConnected(false) {
            if (serverName) {
                m_serverName = serverName;
            }
        }
        
        ~SapAcqDevice() {
            if (m_isConnected) {
                Destroy();
            }
        }
        
        bool Create() {
            // Always allow connection - don't limit the number of devices
            // Just increment counter and connect
            s_deviceCount++;
            m_isConnected = true;
            return true;
        }
        
        bool Destroy() {
            if (m_isConnected) {
                s_deviceCount--;
                m_isConnected = false;
            }
            return true;
        }
        
        SapLocation GetLocation() { return SapLocation(); }
        
        bool IsFeatureAvailable(const char* featureName, BOOL* pAvailable) {
            *pAvailable = TRUE;
            return true;
        }
        
        bool GetFeatureInfo(const char* featureName, SapFeature* pFeature) {
            return true;
        }
        
        bool GetFeatureValue(const char* featureName, char* valueStr, size_t maxLen) {
            if (strcmp(featureName, "DeviceModelName") == 0) {
                if (!m_serverName.empty()) {
                    strncpy(valueStr, m_serverName.c_str(), maxLen);
                } else {
                    strncpy(valueStr, "Nano-C4020", maxLen);
                }
            } else if (strcmp(featureName, "DeviceSerialNumber") == 0) {
                strncpy(valueStr, "SN12345678", maxLen);
            } else if (strcmp(featureName, "DeviceFirmwareVersion") == 0) {
                strncpy(valueStr, "1.0.0", maxLen);
            } else if (strcmp(featureName, "DeviceUserID") == 0) {
                strncpy(valueStr, "Camera", maxLen);
            } else {
                strncpy(valueStr, "Unknown", maxLen);
            }
            return true;
        }
        
        bool GetFeatureValue(const char* featureName, double* pValue) {
            if (strcmp(featureName, "ExposureTime") == 0) {
                *pValue = 10000.0;
            }
            return true;
        }
        
        bool SetFeatureValue(const char* featureName, const char* valueStr) {
            return true;
        }
        
        bool SetFeatureValue(const char* featureName, double value) {
            return true;
        }
    };
    
    // Buffer classes
    class SapBuffer {
    public:
        enum BufferType {
            TypeDefault = 0,
            TypeContiguous = 1,
            TypeScatterGather = 2
        };
        
        SapBuffer() {}
        virtual ~SapBuffer() {}
        
        virtual bool Create() { return true; }
        virtual bool Destroy() { return true; }
        
        UINT32 GetWidth() { return 640; }
        UINT32 GetHeight() { return 480; }
        UINT32 GetPitch() { return 640; }
        UINT32 GetFormat() { return SapFormatConstants::SAPBUFFER_FORMAT_MONO8; }
        UINT32 GetIndex() { return 0; }
        
        // Return a pointer to buffer data
        void* GetAddress() { 
            static std::vector<uint8_t> dummyData(640 * 480, 128);
            return dummyData.data();
        }
        
        // This version takes a pointer-to-pointer, to match some Sapera API styles
        bool GetAddress(void** ppData) { 
            static std::vector<uint8_t> dummyData(640 * 480, 128);
            *ppData = dummyData.data();
            return true; 
        }
    };
    
    class SapBufferWithTrash : public SapBuffer {
    public:
        SapBufferWithTrash() {}
        SapBufferWithTrash(int count, SapAcqDevice* pDevice) {}
    };
    
    // Callback information class
    class SapXferCallbackInfo {
    private:
        void* m_pContext;
        
    public:
        SapXferCallbackInfo(void* pContext) : m_pContext(pContext) {}
        void* GetContext() { return m_pContext; }
    };
    
    // Transfer classes
    class SapTransfer {
    protected:
        XferCallbackFunc m_pCallback;
        void* m_pContext;
        
    public:
        SapTransfer() : m_pCallback(nullptr), m_pContext(nullptr) {}
        virtual ~SapTransfer() {}
        
        virtual bool Create() { return true; }
        virtual bool Destroy() { return true; }
        
        virtual bool Grab() { return true; }
        virtual bool Freeze() { return true; }
        virtual bool Wait(int timeout) { return true; }
        
        // Add the missing callback method
        virtual void SetCallbackInfo(XferCallbackFunc pCallback, void* pContext) {
            m_pCallback = pCallback;
            m_pContext = pContext;
        }
    };
    
    class SapAcqDeviceToBuf : public SapTransfer {
    private:
        SapAcqDevice* m_pDevice;
        SapBuffer* m_pBuffer;
        bool m_isSpecialHandling;
        
    public:
        SapAcqDeviceToBuf() : m_pDevice(nullptr), m_pBuffer(nullptr), m_isSpecialHandling(false) {}
        
        SapAcqDeviceToBuf(SapAcqDevice* pDevice, SapBuffer* pBuffer) 
            : m_pDevice(pDevice), m_pBuffer(pBuffer), m_isSpecialHandling(false) 
        {
            // In a robust implementation, we don't have special cases for specific camera indices
            // All cameras should be treated equally unless they have different capabilities
            // which should be detected dynamically
        }
        
        // Override base methods
        bool Create() override {
            // Use base implementation for all cameras
            return SapTransfer::Create();
        }
        
        bool Grab() override {
            // Use base implementation for grabbing
            bool success = SapTransfer::Grab();
            
            // If successful and we have a callback, invoke it
            if (success && m_pCallback && m_pContext) {
                SapXferCallbackInfo info(m_pContext);
                m_pCallback(&info);
            }
            
            return success;
        }
    };
    
    class SapView {
    public:
        SapView() {}
        SapView(SapBuffer* pBuffer, void* hwnd = nullptr) {}
        
        bool Create() { return true; }
        bool Destroy() { return true; }
        
        bool Show() { return true; }
    };
} // namespace SaperaStubs

// Use our stubs
using SapManager = SaperaStubs::SapManager;
using SapLocation = SaperaStubs::SapLocation;
using SapAcqDevice = SaperaStubs::SapAcqDevice;
using SapBuffer = SaperaStubs::SapBuffer;
using SapBufferWithTrash = SaperaStubs::SapBufferWithTrash;
using SapAcqDeviceToBuf = SaperaStubs::SapAcqDeviceToBuf;
using SapTransfer = SaperaStubs::SapTransfer;
using SapXferCallbackInfo = SaperaStubs::SapXferCallbackInfo;
using SapView = SaperaStubs::SapView;
using SapFeature = SaperaStubs::SapFeature;
using SapManVersionInfo = SaperaStubs::SapManVersionInfo;

// Expose buffer format constants at global scope
constexpr int SAPBUFFER_FORMAT_MONO8 = SaperaStubs::SapFormatConstants::SAPBUFFER_FORMAT_MONO8;
constexpr int SAPBUFFER_FORMAT_MONO16 = SaperaStubs::SapFormatConstants::SAPBUFFER_FORMAT_MONO16;
constexpr int SAPBUFFER_FORMAT_RGB24 = SaperaStubs::SapFormatConstants::SAPBUFFER_FORMAT_RGB24;
constexpr int SAPBUFFER_FORMAT_RGB32 = SaperaStubs::SapFormatConstants::SAPBUFFER_FORMAT_RGB32;

// Expose CORSERVER_MAX_STRLEN at global scope to fix the compilation error
constexpr int CORSERVER_MAX_STRLEN = SaperaStubs::CORSERVER_MAX_STRLEN;

namespace cam_matrix::core {

class SaperaUtils {
public:
    // Check if Sapera SDK is available and properly initialized
    static bool isSaperaAvailable() {
        return true;
    }
    
    // Check if GigE Vision is available
    static bool isGigeVisionAvailable() {
        return true;
    }
    
    // Check if direct camera access is available
    static bool isDirectAccessAvailable() {
        return true;
    }
    
    // Get cameras available for direct access
    static std::vector<std::string> getDirectAccessCameras() {
        std::vector<std::string> cameraNames;
        getAvailableCameras(cameraNames);
        return cameraNames;
    }

    // Get the Sapera SDK version
    static std::string getSaperaVersion() {
        return "Sapera SDK 8.70.0.1";
    }
    
    // Get the GigE Vision version
    static std::string getGigeVisionVersion() {
        return "GigE Vision 2.0.0";
    }
    
    // Get available cameras
    static bool getAvailableCameras(std::vector<std::string>& cameraNames) {
        cameraNames.clear();
        
        // Return all cameras in the system
        int serverCount = SapManager::GetServerCount();
        for (int i = 0; i < serverCount; ++i) {
            char serverName[CORSERVER_MAX_STRLEN];
            if (SapManager::GetServerName(i, serverName, sizeof(serverName))) {
                // Skip excluded cameras
                if (!shouldExcludeCamera(serverName)) {
                    cameraNames.push_back(serverName);
                }
            }
        }
        
        return !cameraNames.empty();
    }

    // Helper method to determine if a camera should be excluded from the list
    static bool shouldExcludeCamera(const std::string& cameraName) {
        // Exclude system cameras
        if (cameraName == "System" || cameraName == "Default") {
            return true;
        }
        
        // Include all other cameras
        return false;
    }
};

} // namespace cam_matrix::core
