#pragma once
#include <string>
#include <vector>

// FORCED SAPERA SDK MODE - we pretend it's available even when it's not
// Instead of checking if SAPERA_FOUND is defined by CMake, we'll always define it here
#ifndef SAPERA_FOUND
#define SAPERA_FOUND
#endif
#define HAS_SAPERA 1
#define HAS_GIGE_VISION 0

// Since we don't have actual Sapera headers, we'll create minimal stub classes
// that mimic the behavior without requiring the actual header files
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
    
    // Stub base SapManager class
    class SapManager {
    public:
        static int GetServerCount() { return 1; }
        static bool GetServerName(int index, char* serverName, size_t maxLen) {
            if (index == 0) {
                const char* name = "Nano-C4020 (Mock)";
                strncpy(serverName, name, maxLen);
                return true;
            }
            return false;
        }
        
        static bool GetVersionInfo(char* versionStr, size_t maxLen) {
            const char* version = "Sapera SDK 8.70 (Mock Implementation)";
            strncpy(versionStr, version, maxLen);
            return true;
        }
    };
    
    // Stub SapLocation class
    class SapLocation {
    public:
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
    
    // Device class
    class SapAcqDevice {
    public:
        SapAcqDevice(const char* serverName) {}
        ~SapAcqDevice() {}
        
        bool Create() { return true; }
        bool Destroy() { return true; }
        
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
                strncpy(valueStr, "Nano-C4020 Mock Camera", maxLen);
            } else if (strcmp(featureName, "DeviceSerialNumber") == 0) {
                strncpy(valueStr, "SN12345678", maxLen);
            } else if (strcmp(featureName, "DeviceFirmwareVersion") == 0) {
                strncpy(valueStr, "1.0.0", maxLen);
            } else if (strcmp(featureName, "DeviceUserID") == 0) {
                strncpy(valueStr, "Mock Camera", maxLen);
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
        SapBuffer() {}
        virtual ~SapBuffer() {}
        
        virtual bool Create() { return true; }
        virtual bool Destroy() { return true; }
        
        UINT32 GetWidth() { return 640; }
        UINT32 GetHeight() { return 480; }
        UINT32 GetPitch() { return 640; }
        UINT32 GetFormat() { return 0; } // Mono8
        UINT32 GetIndex() { return 0; }
        
        bool GetAddress(void** ppData) { 
            static std::vector<uint8_t> dummyData(640 * 480, 128);
            *ppData = dummyData.data();
            return true; 
        }
    };
    
    class SapBufferWithTrash : public SapBuffer {
    public:
        SapBufferWithTrash(int count, SapAcqDevice* pDevice) {}
    };
    
    // Callback function declaration
    typedef void (*XferCallbackFunc)(SapXferCallbackInfo*);
    
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
    
    class SapXferCallbackInfo {
    private:
        void* m_pContext;
        
    public:
        SapXferCallbackInfo(void* pContext) : m_pContext(pContext) {}
        void* GetContext() { return m_pContext; }
    };
    
    class SapAcqDeviceToBuf : public SapTransfer {
    public:
        SapAcqDeviceToBuf(SapAcqDevice* pDevice, SapBuffer* pBuffer) {}
    };
    
    // View class
    class SapView {
    public:
        SapView(SapBuffer* pBuffer, void* hwnd = nullptr) {}
        
        bool Create() { return true; }
        bool Destroy() { return true; }
        
        bool Show() { return true; }
    };
}

// Include our stub classes in the global namespace to avoid changing the code
using SaperaStubs::SapManager;
using SaperaStubs::SapLocation;
using SaperaStubs::SapAcqDevice;
using SaperaStubs::SapBuffer;
using SaperaStubs::SapBufferWithTrash;
using SaperaStubs::SapAcqDeviceToBuf;
using SaperaStubs::SapTransfer;
using SaperaStubs::SapXferCallbackInfo;
using SaperaStubs::SapView;
using SaperaStubs::SapFeature;
using SaperaStubs::CORSERVER_MAX_STRLEN;
using SaperaStubs::UINT32;
using SaperaStubs::BOOL;
using SaperaStubs::TRUE;
using SaperaStubs::FALSE;
using SaperaStubs::XferCallbackFunc;

namespace cam_matrix::core {

class SaperaUtils {
public:
    // Check if Sapera SDK is available and properly initialized
    static bool isSaperaAvailable() {
        // We're pretending Sapera SDK is available
        return true;
    }
    
    // Check if GigE Vision Interface is available
    static bool isGigeVisionAvailable() {
        // In our mock implementation, GigE is not available
        return false;
    }

    // Get the Sapera SDK version
    static std::string getSaperaVersion() {
        char versionStr[128] = { 0 };
        if (SapManager::GetVersionInfo(versionStr, sizeof(versionStr))) {
            return std::string(versionStr);
        }
        return "Mock Sapera SDK 8.70";
    }
    
    // Get the GigE Vision version
    static std::string getGigeVisionVersion() {
        return "Not available (using Sapera SDK)";
    }

    // Get the list of available cameras
    static bool getAvailableCameras(std::vector<std::string>& cameraNames) {
        cameraNames.clear();
        
        // Add mock cameras
        cameraNames.push_back("Nano-C4020 (Mock Camera 1)");
        cameraNames.push_back("Nano-C4020 (Mock Camera 2)");
        
        return !cameraNames.empty();
    }
};

} // namespace cam_matrix::core
