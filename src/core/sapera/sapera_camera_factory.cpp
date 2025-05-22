#include "sapera_camera_factory.hpp"
#include <QDebug>

namespace cam_matrix::core::sapera
{

    std::shared_ptr<SaperaCameraBase> SaperaCameraFactory::createCamera(
        CameraMode mode,
        int serverIndex,
        const QString &serverName,
        const QString &configName)
    {
        // Create the appropriate camera type based on the mode
        if (mode == CameraMode::SingleCamera)
        {
            qDebug() << "Creating single camera instance for" << serverName;
            return std::make_shared<SaperaSingleCamera>(serverIndex, serverName, configName);
        }
        else // MultiCamera mode
        {
            qDebug() << "Creating multi-camera instance for" << serverName;
            return std::make_shared<SaperaMultiCamera>(serverIndex, serverName, configName);
        }
    }

    std::vector<QString> SaperaCameraFactory::enumerateCameras()
    {
        // Delegate to the base class implementation
        return SaperaCameraBase::enumerateCameras();
    }

} // namespace cam_matrix::core::sapera