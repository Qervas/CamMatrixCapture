#pragma once

#include "sapera_camera_base.hpp"
#include "sapera_single_camera.hpp"
#include "sapera_multi_camera.hpp"
#include <memory>
#include <QString>

namespace cam_matrix::core::sapera
{
    enum class CameraMode
    {
        SingleCamera, // For live view with continuous acquisition
        MultiCamera   // For synchronized capture without continuous streaming
    };

    class SaperaCameraFactory
    {
    public:
        // Create a camera of the specified mode
        static std::shared_ptr<SaperaCameraBase> createCamera(
            CameraMode mode,
            int serverIndex,
            const QString &serverName,
            const QString &configName);

        // Get list of available cameras
        static std::vector<QString> enumerateCameras();
    };

} // namespace cam_matrix::core::sapera