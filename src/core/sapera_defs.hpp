#pragma once
#include <string>
#include <vector>

// Let CMake decide if we should use the real Sapera SDK or our mock implementation
#if defined(SAPERA_FOUND) && SAPERA_FOUND && defined(HAS_SAPERA) && HAS_SAPERA
    // Use real Sapera SDK headers
    #include "SapClassBasic.h"
#else
    // No SAPERA SDK found - use our custom definitions
    #define HAS_SAPERA 0
    #define HAS_GIGE_VISION 1

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
        
        // Callback function declaration - defined before SapXferCallbackInfo
        class SapXferCallbackInfo;
        typedef void (*XferCallbackFunc)(SapXferCallbackInfo*);
        
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
            SapAcqDevice() {}
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
        public:
            SapAcqDeviceToBuf() {}
            SapAcqDeviceToBuf(SapAcqDevice* pDevice, SapBuffer* pBuffer) {}
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
    
    // Use our stubs instead of the real Sapera types
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
    
    // Define some constants as preprocessor macros
    #define SAPBUFFER_FORMAT_MONO8 0
    #define SAPBUFFER_FORMAT_MONO16 1
    #define SAPBUFFER_FORMAT_RGB24 2
    #define SAPBUFFER_FORMAT_RGB32 3
#endif

namespace cam_matrix::core {

class SaperaUtils {
public:
    // Check if Sapera SDK is available and properly initialized
    static bool isSaperaAvailable() {
        #if defined(SAPERA_FOUND)
            return true;
        #else
            return false;
        #endif
    }
    
    // Check if GigE Vision Interface is available
    static bool isGigeVisionAvailable() {
        #if defined(HAS_GIGE_VISION) && HAS_GIGE_VISION
            return true;
        #else
            return false;
        #endif
    }

    // Get the Sapera SDK version
    static std::string getSaperaVersion() {
        #if defined(SAPERA_FOUND)
            char versionStr[128] = { 0 };
            int major=0, minor=0, revision=0, build=0;
            
            #ifdef SAPERA_LT_VERSION_MAJOR
                major = SAPERA_LT_VERSION_MAJOR;
                minor = SAPERA_LT_VERSION_MINOR;
                revision = SAPERA_LT_REVISION_NUMBER;
                build = SAPERA_LT_BUILD_NUMBER;
                sprintf(versionStr, "Sapera LT %d.%d.%d.%d", major, minor, revision, build);
            #else
                // Try to get version through API
                if (SapManager::GetVersionInfo(versionStr, sizeof(versionStr))) {
                    return std::string(versionStr);
                }
                return "Sapera SDK (version unknown)";
            #endif
            
            return std::string(versionStr);
        #else
            return "Mock Sapera SDK 8.70";
        #endif
    }
    
    // Get the GigE Vision version
    static std::string getGigeVisionVersion() {
        #if defined(HAS_GIGE_VISION) && HAS_GIGE_VISION
            return "Teledyne GigE Vision Interface";
        #else
            return "Not available";
        #endif
    }

    // Get the list of available cameras
    static bool getAvailableCameras(std::vector<std::string>& cameraNames) {
        cameraNames.clear();
        
        #if defined(SAPERA_FOUND)
            // Get the number of servers (cameras)
            int serverCount = SapManager::GetServerCount();
            
            for (int i = 0; i < serverCount; i++) {
                char serverName[CORSERVER_MAX_STRLEN];
                if (SapManager::GetServerName(i, serverName, sizeof(serverName))) {
                    cameraNames.push_back(serverName);
                }
            }
            
            return !cameraNames.empty();
        #else
            // Add mock cameras
            cameraNames.push_back("Nano-C4020 (Mock Camera 1)");
            cameraNames.push_back("Nano-C4020 (Mock Camera 2)");
            
            return !cameraNames.empty();
        #endif
    }
};

} // namespace cam_matrix::core
