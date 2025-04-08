#include "core/sapera_manager.hpp"
#include <QDebug>

namespace cam_matrix::core {

SaperaManager::SaperaManager() {
    // Check if Sapera is installed
    if (!isSaperaInstalled()) {
        emit error("Sapera SDK not installed");
    } else {
        // Initial scan for cameras
        scanForCameras();
    }
}

SaperaManager::~SaperaManager() {
    // Clean up - cameras will be destroyed automatically
}

bool SaperaManager::scanForCameras() {
    emit statusChanged("Scanning for Sapera cameras...");

    // Clear existing cameras
    cameras_.clear();

    // Check for Sapera installation
    if (!isSaperaInstalled()) {
        emit error("Sapera SDK not installed");
        return false;
    }

#if HAS_SAPERA
    try {
        // Count the number of servers
        int serverCount = SapManager::GetServerCount();
        emit statusChanged("Found " + std::to_string(serverCount) + " Sapera devices");

        // Enumerate all servers
        for (int i = 0; i < serverCount; i++) {
            char serverName[CORSERVER_MAX_STRLEN];
            if (SapManager::GetServerName(i, serverName, sizeof(serverName))) {
                // Create a camera object for each server
                auto camera = std::make_shared<SaperaCamera>(serverName);

                // Connect status signals
                connect(camera.get(), &SaperaCamera::statusChanged,
                        this, &SaperaManager::statusChanged);
                connect(camera.get(), &SaperaCamera::error,
                        this, &SaperaManager::error);

                cameras_.push_back(camera);

                emit statusChanged("Added camera: " + std::string(serverName));
            }
        }

        emit camerasChanged();
        return true;
    } catch (const std::exception& e) {
        emit error(std::string("Exception scanning for cameras: ") + e.what());
        return false;
    }
#else
    emit error("Sapera SDK not available in this build");
    return false;
#endif
}

std::vector<std::shared_ptr<Camera>> SaperaManager::getCameras() const {
    std::vector<std::shared_ptr<Camera>> result;
    for (const auto& camera : cameras_) {
        result.push_back(camera);
    }
    return result;
}

std::shared_ptr<Camera> SaperaManager::getCameraByIndex(size_t index) const {
    if (index < cameras_.size()) {
        return cameras_[index];
    }
    return nullptr;
}

std::shared_ptr<SaperaCamera> SaperaManager::getSaperaCameraByIndex(size_t index) const {
    if (index < cameras_.size()) {
        return cameras_[index];
    }
    return nullptr;
}

bool SaperaManager::isSaperaInstalled() {
#if HAS_SAPERA
    // First check the compile-time flag
    char* saperadir = NULL;
    size_t len = 0;
    _dupenv_s(&saperadir, &len, "SAPERADIR");
    if (!saperadir) {
        return false;
    }
    free(saperadir);

    // Also verify that we can call a basic Sapera function
    try {
        int serverCount = SapManager::GetServerCount();
        return true;
    }
    catch (...) {
        return false;  // Exception occurred when trying to use Sapera
    }
#else
    return false;
#endif
}

} // namespace cam_matrix::core
