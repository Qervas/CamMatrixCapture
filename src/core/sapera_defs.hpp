#pragma once
#include <string>
#include <vector>
#include <cstring> // For strncpy

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
    using SapManVersionInfo = SaperaStubs::SapManVersionInfo;
    
    // Expose buffer format constants at global scope
    constexpr int SAPBUFFER_FORMAT_MONO8 = SaperaStubs::SapFormatConstants::SAPBUFFER_FORMAT_MONO8;
    constexpr int SAPBUFFER_FORMAT_MONO16 = SaperaStubs::SapFormatConstants::SAPBUFFER_FORMAT_MONO16;
    constexpr int SAPBUFFER_FORMAT_RGB24 = SaperaStubs::SapFormatConstants::SAPBUFFER_FORMAT_RGB24;
    constexpr int SAPBUFFER_FORMAT_RGB32 = SaperaStubs::SapFormatConstants::SAPBUFFER_FORMAT_RGB32;
#endif

namespace cam_matrix::core {

class SaperaUtils {
public:
    // Check if Sapera SDK is available and properly initialized
    static bool isSaperaAvailable() {
        #if defined(SAPERA_FOUND) && SAPERA_FOUND
            return true;
        #else
            return false;
        #endif
    }
    
    // Check if GigE Vision is available
    static bool isGigeVisionAvailable() {
        #if defined(HAS_GIGE_VISION) && HAS_GIGE_VISION
            return true;
        #else
            return false;
        #endif
    }
    
    // Get the Sapera SDK version
    static std::string getSaperaVersion() {
        #if defined(SAPERA_FOUND) && SAPERA_FOUND
            SapManVersionInfo versionInfo;
            if (SapManager::GetVersionInfo(&versionInfo)) {
                char versionStr[256] = {0};
                // Use accessor methods instead of direct member access
                sprintf(versionStr, "Sapera SDK %d.%d.%d.%d", 
                        versionInfo.GetMajor(), versionInfo.GetMinor(), 
                        versionInfo.GetRevision(), versionInfo.GetBuild());
                return versionStr;
            }
            return "Sapera SDK (Unknown Version)";
        #else
            return "Sapera SDK Not Available (Stub Implementation)";
        #endif
    }
    
    // Get the GigE Vision version
    static std::string getGigeVisionVersion() {
        #if defined(HAS_GIGE_VISION) && HAS_GIGE_VISION
            return "GigE Vision Simulation Mode";
        #else
            return "GigE Vision Not Available";
        #endif
    }
    
    // Get available cameras
    static bool getAvailableCameras(std::vector<std::string>& cameraNames) {
        cameraNames.clear();
        
        #if defined(SAPERA_FOUND) && SAPERA_FOUND
            // Query real cameras from Sapera SDK
            int serverCount = SapManager::GetServerCount();
            for (int i = 0; i < serverCount; ++i) {
                char serverName[CORSERVER_MAX_STRLEN];
                if (SapManager::GetServerName(i, serverName, sizeof(serverName))) {
                    cameraNames.push_back(serverName);
                }
            }
        #else
            // Return some simulated cameras
            cameraNames.push_back("Nano-C4020 (Mock)");
            cameraNames.push_back("GigE-1800 (Mock)");
            cameraNames.push_back("FiberCam-6K (Mock)");
        #endif
        
        return !cameraNames.empty();
    }
};

} // namespace cam_matrix::core
