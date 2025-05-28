#pragma once
#include <SapClassBasic.h>
#include <SapBufferWithTrash.h>
#include <QObject>
#include <QImage>
#include <QString>
#include <vector>
#include <string>

namespace cam_matrix::core::sapera
{

    // Abstract base class for all Sapera cameras
    class SaperaCameraBase : public QObject
    {
        Q_OBJECT
    public:
        SaperaCameraBase(int serverIndex, const QString &serverName, const QString &configName, QObject *parent = nullptr);
        virtual ~SaperaCameraBase();

        // Common interface for all camera types
        virtual bool connectCamera() = 0;
        virtual void disconnectCamera();
        virtual bool capturePhoto(const QString &filePath) = 0;
        virtual QImage getLatestFrame() const;

        // High-quality photo capture (with default implementation that calls regular capturePhoto)
        virtual bool captureHighQualityPhoto(const QString &filePath, const QString &format = "tiff")
        {
            // Base implementation just forwards to regular capture for compatibility
            return capturePhoto(filePath);
        }

        // Common accessors
        QString getServerName() const { return m_serverName; }
        bool isConnected() const { return m_connected; }

        // Static camera enumeration
        static std::vector<QString> enumerateCameras();

    signals:
        void frameReady(const QImage &);
        void statusChanged(const QString &message);
        void errorOccurred(const QString &error);

    protected:
        // Common cleanup method
        virtual void cleanup();

        // Shared resources
        int m_serverIndex;
        QString m_serverName;
        QString m_configName;
        bool m_connected = false;
        bool m_connFailed = false;

        // Sapera objects
        SapLocation *m_loc = nullptr;
        SapAcqDevice *m_acqDevice = nullptr;
        SapBuffer *m_rawBuffer = nullptr;
        SapBuffer *m_rgbBuffer = nullptr;
        SapColorConversion *m_colorConv = nullptr;
        SapAcqDeviceToBuf *m_transfer = nullptr;

        // Frame storage
        QImage m_lastFrame;
    };

} // namespace cam_matrix::core::sapera