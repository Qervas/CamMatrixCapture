#pragma once

// Check if Sapera SDK is available
#if defined(SAPERA_FOUND)
    #include <SapClassBasic.h>
    #define HAS_SAPERA 1

namespace cam_matrix::core {

class SaperaUtils {
public:
    // Check if Sapera SDK is available and properly initialized
    static bool isSaperaAvailable() {
        return SapManager::IsAvailable();
    }

    // Get the Sapera SDK version
    static std::string getSaperaVersion() {
        UINT32 major, minor, revision, build;
        SapManager::GetVersionInfo(&major, &minor, &revision, &build);
        return std::to_string(major) + "." + 
               std::to_string(minor) + "." + 
               std::to_string(revision) + "." + 
               std::to_string(build);
    }

    // Get the list of available cameras
    static bool getAvailableCameras(std::vector<std::string>& cameraNames) {
        return SapManager::GetResourceList(SapManager::ResourceAcqDevice, &cameraNames);
    }
};

} // namespace cam_matrix::core

#else
    #define HAS_SAPERA 0

namespace cam_matrix::core {

class SaperaUtils {
public:
    // Check if Sapera SDK is available and properly initialized
    static bool isSaperaAvailable() {
        return false;
    }

    // Get the Sapera SDK version
    static std::string getSaperaVersion() {
        return "Not available";
    }

    // Get the list of available cameras
    static bool getAvailableCameras(std::vector<std::string>& cameraNames) {
        cameraNames.clear();
        return false;
    }
};

} // namespace cam_matrix::core

#endif
