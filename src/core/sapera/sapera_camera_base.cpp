#include "sapera_camera_base.hpp"
#include "sapera_utils.hpp"
#include <QDebug>
#include <QCoreApplication>
#include <QFileInfo>

namespace cam_matrix::core::sapera
{

    SaperaCameraBase::SaperaCameraBase(int serverIndex, const QString &serverName, const QString &configName, QObject *parent)
        : QObject(parent), m_serverIndex(serverIndex), m_serverName(serverName), m_configName(configName)
    {
        // Create the location with server name
        m_loc = new SapLocation(serverName.toStdString().c_str(), 0);

        // Disable error dialogs
        SapManager::SetDisplayStatusMode(FALSE);
    }

    SaperaCameraBase::~SaperaCameraBase()
    {
        cleanup();
        delete m_loc;
    }

    void SaperaCameraBase::cleanup()
    {
        // Disable error dialogs during cleanup
        SapManager::SetDisplayStatusMode(FALSE);

        try
        {
            // Stop and clean up all resources - order matters!
            if (m_transfer)
            {
                delete m_transfer;
                m_transfer = nullptr;
            }

            if (m_colorConv)
            {
                m_colorConv->Destroy();
                delete m_colorConv;
                m_colorConv = nullptr;
            }

            if (m_rawBuffer)
            {
                m_rawBuffer->Destroy();
                delete m_rawBuffer;
                m_rawBuffer = nullptr;
            }

            if (m_acqDevice)
            {
                m_acqDevice->Destroy();
                delete m_acqDevice;
                m_acqDevice = nullptr;
            }

            m_connected = false;
            m_connFailed = false;

            qDebug() << "Base cleanup completed for:" << m_serverName;
        }
        catch (...)
        {
            qWarning() << "Unhandled exception during cleanup";
            // Reset all pointers to nullptr to prevent double-free attempts
            m_transfer = nullptr;
            m_colorConv = nullptr;
            m_rawBuffer = nullptr;
            m_acqDevice = nullptr;
            m_connected = false;
        }
    }

    void SaperaCameraBase::disconnectCamera()
    {
        cleanup();
    }

    QImage SaperaCameraBase::getLatestFrame() const
    {
        // Default implementation just returns the last stored frame
        return m_lastFrame;
    }

    std::vector<QString> SaperaCameraBase::enumerateCameras()
    {
        std::vector<QString> result;

        try
        {
            // First try standard Sapera enumeration
            SapManager::SetDisplayStatusMode(FALSE);
            int count = SapManager::GetServerCount();
            qDebug() << "Found" << count << "Sapera servers";

            for (int i = 0; i < count; ++i)
            {
                char name[256];
                if (SapManager::GetServerName(i, name, sizeof(name)))
                {
                    QString serverName = QString::fromUtf8(name);
                    qDebug() << "Found server:" << serverName;
                    result.push_back(serverName);
                }
            }

            // If no cameras found or count is zero, use same naming pattern as the sample code
            if (result.empty())
            {
                qDebug() << "No cameras found via Sapera enumeration. Using sample code naming pattern.";

                // Use the exact naming pattern from the sample code: Nano-C4020_N
                const int acqDeviceNumber = 12;
                for (int i = 1; i <= acqDeviceNumber; i++)
                {
                    // IMPORTANT: Use the EXACT same format as the sample code
                    QString cameraName = QString("Nano-C4020_%1").arg(i);
                    qDebug() << "Adding camera with pattern name:" << cameraName;
                    result.push_back(cameraName);
                }
            }
        }
        catch (const std::exception &e)
        {
            qWarning() << "Exception during camera enumeration:" << e.what();
        }
        catch (...)
        {
            qWarning() << "Unknown exception during camera enumeration";
        }

        return result;
    }

} // namespace cam_matrix::core::sapera