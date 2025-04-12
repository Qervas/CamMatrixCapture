#include "core/simulated_camera_manager.hpp"
#include <QtMath>

namespace cam_matrix::core {

SimulatedCameraManager::SimulatedCameraManager()
    : simulator_(std::make_unique<CameraSimulator>())
{
    // Create default 4 cameras with 30fps
    setSimulatorParams(4, 30, true, 0);
}

SimulatedCameraManager::~SimulatedCameraManager() {
    disconnectAllCameras();
}

bool SimulatedCameraManager::scanForCameras() {
    // First disconnect all existing cameras
    disconnectAllCameras();

    // Clear our storage of cameras
    cameras_.clear();

    // Get reference to the cameras from the simulator
    auto simCameras = simulator_->createCameraMatrix(2, 2); // 2x2 grid by default

    // Store cameras in base class format (now using Camera as the base type)
    for (const auto& camera : simCameras) {
        cameras_.push_back(camera);  // This should work now
    }

    emit camerasChanged();
    return true;
}

void SimulatedCameraManager::setSimulatorParams(int numCameras, int fps, bool synchronizedMode, int jitterMs) {
    // Disconnect and clear existing cameras
    disconnectAllCameras();
    cameras_.clear();

    // Configure simulator
    simulator_->setFrameRate(fps);
    simulator_->setJitter(jitterMs);
    simulator_->setSimulationMode(synchronizedMode);

    // Calculate grid layout - try to make it as square as possible
    int cols = qCeil(qSqrt(numCameras));
    int rows = qCeil(static_cast<double>(numCameras) / cols);

    // Create cameras
    auto simCameras = simulator_->createCameraMatrix(rows, cols);

    // Keep only the requested number
    while (simCameras.size() > numCameras && !simCameras.empty()) {
        simCameras.pop_back();
    }

    // Store cameras in base class format
    for (const auto& camera : simCameras) {
        cameras_.push_back(camera);  // This should work now
    }

    emit camerasChanged();
}

std::shared_ptr<SimulatedCamera> SimulatedCameraManager::getSimulatedCameraByIndex(size_t index) const {
    if (index < cameras_.size()) {
        return std::dynamic_pointer_cast<SimulatedCamera>(cameras_[index]);
    }
    return nullptr;
}

} // namespace cam_matrix::core
