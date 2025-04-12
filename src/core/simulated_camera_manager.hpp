#pragma once
#include "core/camera_manager.hpp"
#include "core/camera_simulator.hpp"

namespace cam_matrix::core {

class SimulatedCameraManager : public CameraManager {
    Q_OBJECT

public:
    SimulatedCameraManager();
    ~SimulatedCameraManager() override;

    // Override base class methods
    bool scanForCameras() override;

    // Simulator specific configuration
    void setSimulatorParams(int numCameras, int fps, bool synchronizedMode, int jitterMs);
    CameraSimulator* getSimulator() const { return simulator_.get(); }

    // Camera access - extending base class
    std::shared_ptr<SimulatedCamera> getSimulatedCameraByIndex(size_t index) const;

private:
    std::unique_ptr<CameraSimulator> simulator_;
};

} // namespace cam_matrix::core
