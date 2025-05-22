#include "sapera_single_camera.hpp"
#include "sapera_utils.hpp"
#include <QDebug>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QPainter>
#include <QThread>
#include <QtConcurrent/QtConcurrent>

namespace cam_matrix::core::sapera
{

    // QImage conversion helper
    static QImage SapBufferToQImage(SapBuffer *buffer)
    {
        if (!buffer)
        {
            qDebug() << "SapBufferToQImage: Null buffer";
            return QImage();
        }

        int width = buffer->GetWidth();
        int height = buffer->GetHeight();
        int format = buffer->GetFormat();

        // Initialize pData to nullptr
        void *pData = nullptr;

        // Check if GetAddress succeeds and if pData is valid
        bool addressSuccess = buffer->GetAddress(&pData);
        if (!addressSuccess || pData == nullptr)
        {
            qDebug() << "SapBufferToQImage: Failed to get valid buffer address";
            return QImage();
        }

        // Verify we have valid dimensions
        if (width <= 0 || height <= 0)
        {
            qDebug() << "SapBufferToQImage: Invalid dimensions:" << width << "x" << height;
            return QImage();
        }

        if (format == SapFormatRGB888)
        {
            try
            {
                // Create a deep copy to ensure safety
                return QImage(static_cast<const uchar *>(pData), width, height,
                              width * 3, QImage::Format_RGB888)
                    .copy();
            }
            catch (const std::exception &e)
            {
                qDebug() << "SapBufferToQImage: Exception:" << e.what();
                return QImage();
            }
            catch (...)
            {
                qDebug() << "SapBufferToQImage: Unknown exception";
                return QImage();
            }
        }

        qDebug() << "SapBufferToQImage: Unsupported format:" << format;
        return QImage();
    }

    SaperaSingleCamera::SaperaSingleCamera(int serverIndex, const QString &serverName, const QString &configName, QObject *parent)
        : SaperaCameraBase(serverIndex, serverName, configName, parent)
    {
        qDebug() << "Created SingleCamera instance for" << serverName;
    }

    SaperaSingleCamera::~SaperaSingleCamera()
    {
        stopAcquisition();
    }

    bool SaperaSingleCamera::connectCamera()
    {
        try
        {
            // Make sure we clean up any previously created objects first
            cleanup();

            // Initialize Sapera with error suppression
            SapManager::SetDisplayStatusMode(FALSE);
            qDebug() << "Connecting to camera:" << m_serverName << "for live view";

            // Find the configuration file
            QString ccfPath = QCoreApplication::applicationDirPath() + "/NanoC4020.ccf";
            QFileInfo checkFile(ccfPath);

            // Create the acquisition device
            m_acqDevice = new SapAcqDevice(*m_loc, ccfPath.toStdString().c_str());

            // Create buffer for live view (2 buffers for double-buffering)
            m_rawBuffer = new SapBuffer(2, m_acqDevice);

            // Create transfer for continuous acquisition
            m_transfer = new SapAcqDeviceToBuf(m_acqDevice, m_rawBuffer, XferCallback, this);

            qDebug() << "Creating Sapera objects for live view...";

            // Create all objects in sequence
            if (!m_acqDevice->Create())
            {
                qWarning() << "Failed to create acquisition device";
                cleanup();
                return false;
            }

            if (!m_rawBuffer->Create())
            {
                qWarning() << "Failed to create buffer";
                cleanup();
                return false;
            }

            // Create color conversion for live view
            m_colorConv = new SapColorConversion(m_rawBuffer);
            if (m_colorConv->Create())
            {
                m_colorConv->Enable(TRUE, FALSE);
                m_colorConv->SetOutputFormat(SapFormatRGB888);
                m_colorConv->SetAlign(SapColorConversion::AlignRGGB);
                m_colorConv->SetMethod(SapColorConversion::Method1);
                qDebug() << "Color conversion created successfully";
            }
            else
            {
                qWarning() << "Failed to create color conversion";
                delete m_colorConv;
                m_colorConv = nullptr;
            }

            if (!m_transfer->Create())
            {
                qWarning() << "Failed to create transfer";
                cleanup();
                return false;
            }

            if (!m_transfer->Init())
            {
                qWarning() << "Failed to initialize transfer";
                cleanup();
                return false;
            }

            m_connected = true;

            // Start acquisition immediately for live view
            if (!startAcquisition())
            {
                qWarning() << "Failed to start acquisition";
                cleanup();
                return false;
            }

            return true;
        }
        catch (const std::exception &e)
        {
            qWarning() << "Exception in connectCamera:" << e.what();
            cleanup();
            return false;
        }
        catch (...)
        {
            qWarning() << "Unknown exception in connectCamera";
            cleanup();
            return false;
        }
    }

    bool SaperaSingleCamera::startAcquisition()
    {
        if (!m_connected || !m_transfer)
        {
            qWarning() << "Cannot start acquisition - camera not connected";
            return false;
        }

        if (m_isAcquiring)
        {
            qDebug() << "Acquisition already running";
            return true;
        }

        try
        {
            // Start continuous grab for live view
            if (!m_transfer->Grab())
            {
                qWarning() << "Failed to start grab";
                return false;
            }

            m_isAcquiring = true;
            qDebug() << "Live acquisition started for:" << m_serverName;
            return true;
        }
        catch (const std::exception &e)
        {
            qWarning() << "Exception starting acquisition:" << e.what();
            return false;
        }
        catch (...)
        {
            qWarning() << "Unknown exception starting acquisition";
            return false;
        }
    }

    void SaperaSingleCamera::stopAcquisition()
    {
        if (!m_connected || !m_transfer || !m_isAcquiring)
        {
            return;
        }

        try
        {
            m_transfer->Freeze();
            m_transfer->Abort();
            m_isAcquiring = false;
            qDebug() << "Acquisition stopped for:" << m_serverName;
        }
        catch (const std::exception &e)
        {
            qWarning() << "Exception stopping acquisition:" << e.what();
        }
        catch (...)
        {
            qWarning() << "Unknown exception stopping acquisition";
        }
    }

    bool SaperaSingleCamera::capturePhoto(const QString &filePath)
    {
        if (!isConnected())
        {
            qDebug() << "Cannot capture photo - camera not connected";
            return false;
        }

        QString finalPath = filePath;
        if (finalPath.isEmpty())
        {
            // Auto-generate file name with timestamp
            QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
            QString cameraName = m_serverName;
            cameraName.replace(":", "_").replace(" ", "_");
            finalPath = QDir::homePath() + "/Pictures/" + cameraName + "_" + timestamp + ".png";

            // Make sure directory exists
            QFileInfo fileInfo(finalPath);
            QDir dir = fileInfo.dir();
            if (!dir.exists())
            {
                dir.mkpath(".");
            }
        }

        qDebug() << "Starting photo capture to:" << finalPath;
        bool success = false;

        try
        {
            // Use mutex to ensure thread-safety when capturing from multiple cameras
            QImage currentFrame;
            {
                std::lock_guard<std::mutex> lock(m_frameMutex);
                currentFrame = m_lastFrame; // Make a copy while locked
            }

            // If we have a frame, save it directly
            if (!currentFrame.isNull())
            {
                qDebug() << "Saving current live frame";

                // Simply save the frame we already have in memory
                if (currentFrame.save(finalPath, "PNG"))
                {
                    success = true;
                    qDebug() << "Successfully saved photo to" << finalPath;
                    emit statusChanged(tr("Photo captured to %1").arg(finalPath));
                }
                else
                {
                    qWarning() << "Failed to save image to" << finalPath;
                    emit errorOccurred(tr("Failed to save photo to %1").arg(finalPath));
                }
            }
            else
            {
                qWarning() << "No frame available to capture";
                emit errorOccurred(tr("No frame available to save - camera may not be streaming"));
            }
        }
        catch (const std::exception &e)
        {
            qWarning() << "Exception in capturePhoto:" << e.what();
            emit errorOccurred(tr("Error capturing photo: %1").arg(e.what()));
        }
        catch (...)
        {
            qWarning() << "Unknown exception in capturePhoto";
            emit errorOccurred(tr("Unknown error capturing photo"));
        }

        return success;
    }

    void SaperaSingleCamera::capturePhotoAsync(const QString &filePath, const std::function<void(bool, const QString &)> &callback)
    {
        // Launch the photo capture in a separate thread using QtConcurrent
        QtConcurrent::run([this, filePath, callback]()
                          {
            try {
                if (!isConnected()) {
                    qDebug() << "Cannot capture photo asynchronously - camera not connected";
                    if (callback) callback(false, QString());
                    return;
                }

                QString finalPath = filePath;
                if (finalPath.isEmpty()) {
                    // Auto-generate file name with timestamp
                    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
                    QString cameraName = m_serverName;
                    cameraName.replace(":", "_").replace(" ", "_");
                    finalPath = QDir::homePath() + "/Pictures/" + cameraName + "_" + timestamp + ".png";

                    // Make sure directory exists
                    QFileInfo fileInfo(finalPath);
                    QDir dir = fileInfo.dir();
                    if (!dir.exists()) {
                        dir.mkpath(".");
                    }
                }

                qDebug() << "Starting async photo capture to:" << finalPath;
                bool success = false;

                // Use mutex to ensure thread-safety when capturing from multiple cameras
                QImage currentFrame;
                {
                    std::lock_guard<std::mutex> lock(m_frameMutex);
                    currentFrame = m_lastFrame; // Make a copy while locked
                }

                // If we have a frame, save it directly
                if (!currentFrame.isNull()) {
                    qDebug() << "Saving current live frame asynchronously";
                    
                    // Simply save the frame we already have in memory
                    if (currentFrame.save(finalPath, "PNG")) {
                        success = true;
                        qDebug() << "Successfully saved photo to" << finalPath << "(async)";
                        
                        // Use QMetaObject to safely emit signals from a non-GUI thread
                        QMetaObject::invokeMethod(this, [this, finalPath]() {
                            emit statusChanged(tr("Photo captured to %1").arg(finalPath));
                        }, Qt::QueuedConnection);
                    } else {
                        qWarning() << "Failed to save image to" << finalPath << "(async)";
                        QMetaObject::invokeMethod(this, [this, finalPath]() {
                            emit errorOccurred(tr("Failed to save photo to %1").arg(finalPath));
                        }, Qt::QueuedConnection);
                    }
                } else {
                    qWarning() << "No frame available to capture asynchronously";
                    QMetaObject::invokeMethod(this, [this]() {
                        emit errorOccurred(tr("No frame available to save - camera may not be streaming"));
                    }, Qt::QueuedConnection);
                }

                // Call the callback with the result
                if (callback) {
                    callback(success, finalPath);
                }
            }
            catch (const std::exception &e) {
                qWarning() << "Exception in capturePhotoAsync:" << e.what();
                QMetaObject::invokeMethod(this, [this, e]() {
                    emit errorOccurred(tr("Error capturing photo: %1").arg(e.what()));
                }, Qt::QueuedConnection);
                if (callback) callback(false, QString());
            }
            catch (...) {
                qWarning() << "Unknown exception in capturePhotoAsync";
                QMetaObject::invokeMethod(this, [this]() {
                    emit errorOccurred(tr("Unknown error capturing photo"));
                }, Qt::QueuedConnection);
                if (callback) callback(false, QString());
            } });
    }

    void SaperaSingleCamera::XferCallback(SapXferCallbackInfo *pInfo)
    {
        // Disable error dialogs during callback
        SapManager::SetDisplayStatusMode(FALSE);

        if (!pInfo)
        {
            qWarning() << "XferCallback received null info parameter";
            return;
        }

        // Get our camera object from the context
        SaperaSingleCamera *self = nullptr;
        try
        {
            self = static_cast<SaperaSingleCamera *>(pInfo->GetContext());
        }
        catch (...)
        {
            qWarning() << "Exception getting context in XferCallback";
            return;
        }

        if (!self)
        {
            qWarning() << "XferCallback received null context";
            return;
        }

        // Process frame with safe operations
        sapera_utils::safe_sapera_op([&]()
                                     {
        // Lock the frame mutex to prevent concurrent access
        std::lock_guard<std::mutex> lock(self->m_frameMutex);
        
        // First validate objects
        if (!self->m_rawBuffer)
        {
            return false;
        }
        
        // Process with color conversion if available
        if (self->m_colorConv)
        {
            bool conversionSuccess = sapera_utils::safe_sapera_op([&]()
            {
                return self->m_colorConv->Convert();
            }, "color conversion");
            
            if (conversionSuccess)
            {
                SapBuffer* outputBuffer = self->m_colorConv->GetOutputBuffer();
                if (outputBuffer)
                {
                    // Convert to QImage
                    QImage img = SapBufferToQImage(outputBuffer);
                    if (!img.isNull())
                    {
                        self->m_lastFrame = img;
                        emit self->frameReady(img);
                        return true;
                    }
                }
            }
        }
        
        return false; }, "process frame");
    }

} // namespace cam_matrix::core::sapera