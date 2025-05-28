#include "sapera_multi_camera.hpp"
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

    SaperaMultiCamera::SaperaMultiCamera(int serverIndex, const QString &serverName, const QString &configName, QObject *parent)
        : SaperaCameraBase(serverIndex, serverName, configName, parent)
    {
        qDebug() << "Created MultiCamera instance for" << serverName;
    }

    SaperaMultiCamera::~SaperaMultiCamera()
    {
        disconnectCamera();
    }

    bool SaperaMultiCamera::connectCamera()
    {
        try
        {
            // Make sure we clean up any previously created objects first
            cleanup();

            // Initialize Sapera with error suppression
            SapManager::SetDisplayStatusMode(FALSE);
            qDebug() << "Connecting to camera:" << m_serverName << "in multi-camera mode";

            // Find the configuration file
            QString ccfPath = QCoreApplication::applicationDirPath() + "/NanoC4020.ccf";
            QFileInfo checkFile(ccfPath);

            // Create the acquisition device
            m_acqDevice = new SapAcqDevice(*m_loc, ccfPath.toStdString().c_str());

            // Create buffer for snapshot mode (2 buffers for double-buffering)
            m_rawBuffer = new SapBuffer(2, m_acqDevice);

            // Create transfer for taking snapshots
            m_transfer = new SapAcqDeviceToBuf(m_acqDevice, m_rawBuffer, XferCallback, this);

            qDebug() << "Creating Sapera objects for multi-camera mode...";

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

            // Create color conversion (without live view)
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

            // In multi-camera mode, we don't start continuous acquisition
            // This saves resources and prevents multi-camera resource conflicts
            qDebug() << "Camera connected in multi-camera mode (no continuous acquisition)";

            // Create a placeholder image to show in the UI
            QImage placeholderImage(640, 480, QImage::Format_RGB888);
            placeholderImage.fill(Qt::black);
            QPainter painter(&placeholderImage);
            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", 16, QFont::Bold));
            painter.drawText(placeholderImage.rect().adjusted(20, 20, -20, -20),
                             Qt::AlignCenter,
                             tr("Camera: %1\nReady for synchronized capture\nNo live preview in multi-camera mode").arg(m_serverName));
            m_lastFrame = placeholderImage;

            // Emit the placeholder to update UI
            emit frameReady(placeholderImage);
            emit statusChanged(tr("Camera ready for synchronized capture"));

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

    bool SaperaMultiCamera::capturePhoto(const QString &filePath)
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
            // For multi-camera mode, take a snapshot specifically for this capture
            if (m_transfer && m_rawBuffer)
            {
                // Take a single snapshot
                if (m_transfer->Grab())
                {
                    // Wait for transfer to complete
                    if (m_transfer->Wait(4000)) // 4 second timeout
                    {
                        // Apply color conversion if available
                        if (m_colorConv && m_colorConv->Convert())
                        {
                            SapBuffer *outputBuffer = m_colorConv->GetOutputBuffer();
                            if (outputBuffer)
                            {
                                // Save the image
                                if (outputBuffer->Save(finalPath.toLocal8Bit().constData(), "-format png -compression 6"))
                                {
                                    // Create a QImage for preview
                                    QImage capturedImage;
                                    void *pData = nullptr;
                                    if (outputBuffer->GetAddress(&pData) && pData != nullptr)
                                    {
                                        int width = outputBuffer->GetWidth();
                                        int height = outputBuffer->GetHeight();
                                        capturedImage = QImage(static_cast<const uchar *>(pData), width, height,
                                                               width * 3, QImage::Format_RGB888)
                                                            .copy();

                                        // Update last frame and emit signals
                                        m_lastFrame = capturedImage;
                                        emit frameReady(capturedImage);
                                    }

                                    success = true;
                                    emit statusChanged(tr("Photo captured to %1").arg(finalPath));
                                }
                                else
                                {
                                    qWarning() << "Failed to save image";
                                    emit errorOccurred(tr("Failed to save image"));
                                }
                            }
                        }
                    }
                    else
                    {
                        qWarning() << "Timeout waiting for transfer";
                        emit errorOccurred(tr("Timeout waiting for camera transfer"));
                    }
                }
                else
                {
                    qWarning() << "Failed to grab frame";
                    emit errorOccurred(tr("Failed to grab frame from camera"));
                }
            }
            else
            {
                qWarning() << "Transfer or buffer not available";
                emit errorOccurred(tr("Camera resources not available"));
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

    void SaperaMultiCamera::capturePhotoAsync(const QString &filePath, const std::function<void(bool, const QString &)> &callback)
    {
        // Launch the photo capture in a separate thread using QtConcurrent
        QtConcurrent::run([this, filePath, callback]()
                          {
            try {
                if (!isConnected()) {
                    qDebug() << "Cannot capture photo asynchronously - camera not connected";
                    if (callback) callback(false, QString());
                    QMetaObject::invokeMethod(this, [this]() {
                        emit errorOccurred(tr("Camera not connected"));
                    }, Qt::QueuedConnection);
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

                // For multi-camera mode, take a snapshot specifically for this capture
                if (m_transfer && m_rawBuffer)
                {
                    // Take a single snapshot
                    if (m_transfer->Grab())
                    {
                        // Wait for transfer to complete
                        if (m_transfer->Wait(4000)) // 4 second timeout
                        {
                            // Apply color conversion if available
                            if (m_colorConv && m_colorConv->Convert())
                            {
                                SapBuffer* outputBuffer = m_colorConv->GetOutputBuffer();
                                if (outputBuffer)
                                {
                                    // Save the image
                                    if (outputBuffer->Save(finalPath.toLocal8Bit().constData(), "-format png -compression 6"))
                                    {
                                        // Create a QImage for preview
                                        QImage capturedImage;
                                        void* pData = nullptr;
                                        if (outputBuffer->GetAddress(&pData) && pData != nullptr)
                                        {
                                            int width = outputBuffer->GetWidth();
                                            int height = outputBuffer->GetHeight();
                                            capturedImage = QImage(static_cast<const uchar*>(pData), width, height, 
                                                                 width * 3, QImage::Format_RGB888).copy();
                                            
                                            // Update last frame and emit signals safely from non-GUI thread
                                            std::lock_guard<std::mutex> lock(m_frameMutex);
                                            m_lastFrame = capturedImage;
                                            
                                            QMetaObject::invokeMethod(this, [this, capturedImage]() {
                                                emit frameReady(capturedImage);
                                            }, Qt::QueuedConnection);
                                        }
                                        
                                        success = true;
                                        QMetaObject::invokeMethod(this, [this, finalPath]() {
                                            emit statusChanged(tr("Photo captured to %1").arg(finalPath));
                                        }, Qt::QueuedConnection);
                                    }
                                    else
                                    {
                                        QMetaObject::invokeMethod(this, [this]() {
                                            emit errorOccurred(tr("Failed to save image"));
                                        }, Qt::QueuedConnection);
                                    }
                                }
                            }
                        }
                        else
                        {
                            QMetaObject::invokeMethod(this, [this]() {
                                emit errorOccurred(tr("Timeout waiting for camera transfer"));
                            }, Qt::QueuedConnection);
                        }
                    }
                    else
                    {
                        QMetaObject::invokeMethod(this, [this]() {
                            emit errorOccurred(tr("Failed to grab frame from camera"));
                        }, Qt::QueuedConnection);
                    }
                }
                else
                {
                    QMetaObject::invokeMethod(this, [this]() {
                        emit errorOccurred(tr("Camera resources not available"));
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

    void SaperaMultiCamera::XferCallback(SapXferCallbackInfo *pInfo)
    {
        // In multi-camera mode, we don't need continuous callbacks
        // This is only used for the photo capture function

        // Get the camera object from the context
        SaperaMultiCamera *self = static_cast<SaperaMultiCamera *>(pInfo->GetContext());
        if (!self)
        {
            qWarning() << "XferCallback received null context";
            return;
        }

        // We don't need to do any continuous processing here since
        // multi-camera mode doesn't use continuous acquisition
        // The frame processing is done directly in the capturePhoto method
    }

    bool SaperaMultiCamera::captureHighQualityPhoto(const QString &filePath, const QString &format)
    {
        if (!isConnected())
        {
            qDebug() << "Cannot capture high-quality photo - camera not connected";
            return false;
        }

        QString finalPath = filePath;
        if (finalPath.isEmpty())
        {
            // Auto-generate file name with timestamp
            QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
            QString cameraName = m_serverName;
            cameraName.replace(":", "_").replace(" ", "_");
            finalPath = QDir::homePath() + "/Pictures/" + cameraName + "_HQ_" + timestamp + "." + format.toLower();

            // Make sure directory exists
            QFileInfo fileInfo(finalPath);
            QDir dir = fileInfo.dir();
            if (!dir.exists())
            {
                dir.mkpath(".");
            }
        }

        qDebug() << "Starting high-quality photo capture to:" << finalPath;
        bool success = false;

        try
        {
            // For multi-camera mode, the high-quality capture is similar to regular capture
            // since we don't maintain a continuous acquisition

            // Format-specific options
            QString saveOptions;
            if (format.toLower() == "tiff")
            {
                saveOptions = "-format tiff";
            }
            else if (format.toLower() == "png")
            {
                saveOptions = "-format png -compression 1"; // Best quality
            }
            else if (format.toLower() == "bmp")
            {
                saveOptions = "-format bmp";
            }
            else
            {
                // Default to TIFF
                saveOptions = "-format tiff";
            }

            // For multi-camera mode, take a snapshot specifically for this capture
            if (m_transfer && m_rawBuffer)
            {
                // Make sure all existing operations are finished
                m_transfer->Wait(100);

                // Take a single snapshot with increased timeout
                if (m_transfer->Grab())
                {
                    // Wait for transfer to complete with a longer timeout
                    // Resource conflict errors often happen due to timeouts being too short
                    if (m_transfer->Wait(5000)) // 5 second timeout - increased from 4s
                    {
                        // Additional wait to ensure all resources are ready
                        QThread::msleep(100);

                        // Make sure color conversion is optimally configured
                        if (m_colorConv)
                        {
                            m_colorConv->SetOutputFormat(SapFormatRGB888);
                            m_colorConv->SetAlign(SapColorConversion::AlignRGGB);
                            m_colorConv->SetMethod(SapColorConversion::Method1); // High-quality conversion
                        }

                        // Apply color conversion if available
                        if (m_colorConv && m_colorConv->Convert())
                        {
                            SapBuffer *outputBuffer = m_colorConv->GetOutputBuffer();
                            if (outputBuffer)
                            {
                                // Extra wait before save to ensure buffer is fully ready
                                QThread::msleep(50);

                                // Save the image with specified format
                                qDebug() << "Saving high-quality image to" << finalPath << "with options:" << saveOptions;
                                if (outputBuffer->Save(finalPath.toLocal8Bit().constData(), saveOptions.toLocal8Bit().constData()))
                                {
                                    // Create a QImage for preview
                                    QImage capturedImage;
                                    void *pData = nullptr;
                                    if (outputBuffer->GetAddress(&pData) && pData != nullptr)
                                    {
                                        int width = outputBuffer->GetWidth();
                                        int height = outputBuffer->GetHeight();
                                        capturedImage = QImage(static_cast<const uchar *>(pData), width, height,
                                                               width * 3, QImage::Format_RGB888)
                                                            .copy();

                                        // Update last frame and emit signals
                                        m_lastFrame = capturedImage;
                                        emit frameReady(capturedImage);
                                    }

                                    success = true;
                                    emit statusChanged(tr("High-quality photo captured to %1").arg(finalPath));
                                }
                                else
                                {
                                    qWarning() << "Failed to save high-quality image";
                                    emit errorOccurred(tr("Failed to save high-quality image"));
                                }
                            }
                        }
                        else
                        {
                            qWarning() << "Color conversion failed for high-quality capture";
                            emit errorOccurred(tr("Color conversion failed for high-quality capture"));
                        }
                    }
                    else
                    {
                        qWarning() << "Timeout waiting for high-quality capture transfer";
                        emit errorOccurred(tr("Timeout waiting for high-quality capture transfer"));
                    }
                }
                else
                {
                    qWarning() << "Failed to grab high-quality frame";
                    emit errorOccurred(tr("Failed to grab high-quality frame"));
                }
            }
            else
            {
                qWarning() << "Transfer or buffer not available for high-quality capture";
                emit errorOccurred(tr("Camera resources not available for high-quality capture"));
            }
        }
        catch (const std::exception &e)
        {
            qWarning() << "Exception in high-quality photo capture:" << e.what();
            emit errorOccurred(tr("Error in high-quality capture: %1").arg(e.what()));
        }
        catch (...)
        {
            qWarning() << "Unknown exception in high-quality photo capture";
            emit errorOccurred(tr("Unknown error in high-quality capture"));
        }

        return success;
    }

} // namespace cam_matrix::core::sapera