#include <QObject>
#include <QImage>
#include <QString>
#include <QStringList>
#include "sapera_camera_real.hpp"
#include "sapera_utils.hpp"
#include <QDebug>
#include <QBuffer>
#include <functional>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QThread>

// Include the proper SapBufferWithTrash header instead of redefining it
#include <SapBufferWithTrash.h>

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
        qDebug() << "SapBufferToQImage: Failed to get valid buffer address, success:"
                 << addressSuccess << ", pData:" << pData;
        return QImage();
    }

    // Double check pData before using it
    if (!pData)
    {
        qDebug() << "SapBufferToQImage: pData became null after check";
        return QImage();
    }

    // Verify we have valid dimensions before proceeding
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
            qDebug() << "SapBufferToQImage: Exception during QImage creation:" << e.what();
            return QImage();
        }
        catch (...)
        {
            qDebug() << "SapBufferToQImage: Unknown exception during QImage creation";
            return QImage();
        }
    }

    qDebug() << "SapBufferToQImage: Unsupported format:" << format;
    return QImage();
}

SaperaCameraReal::SaperaCameraReal(int serverIndex, const QString &serverName, const QString &configName, QObject *parent)
    : QObject(parent), m_serverIndex(serverIndex), m_serverName(serverName), m_configName(configName)
{
    // Create the location with server name as in the sample code
    m_loc = new SapLocation(serverName.toStdString().c_str(), 0);
}

SaperaCameraReal::~SaperaCameraReal()
{
    cleanup();
    delete m_loc;
}

void SaperaCameraReal::cleanup()
{
    // Disable error dialogs during cleanup
    SapManager::SetDisplayStatusMode(FALSE);

    try
    {
        // Stop and clean up all resources - order matters!
        if (m_transfer)
        {
            try
            {
                if (m_isAcquiring)
                {
                    try
                    {
                        // First try to stop acquisition if it's running
                        m_transfer->Freeze();
                        m_isAcquiring = false;
                        qDebug() << "Stopped acquisition for:" << m_serverName;
                    }
                    catch (...)
                    {
                        qWarning() << "Exception when stopping acquisition - continuing cleanup";
                    }
                }

                try
                {
                    m_transfer->Destroy();
                }
                catch (...)
                {
                    qWarning() << "Exception when destroying transfer object - continuing cleanup";
                }

                delete m_transfer;
            }
            catch (...)
            {
                qWarning() << "Exception when deleting transfer object";
            }
            m_transfer = nullptr;
        }

        if (m_colorConv)
        {
            try
            {
                m_colorConv->Destroy();
                delete m_colorConv;
            }
            catch (...)
            {
                qWarning() << "Exception when cleaning up color conversion object";
            }
            m_colorConv = nullptr;
        }

        if (m_rawBuffer)
        {
            try
            {
                m_rawBuffer->Destroy();
                delete m_rawBuffer;
            }
            catch (...)
            {
                qWarning() << "Exception when cleaning up buffer object";
            }
            m_rawBuffer = nullptr;
        }

        if (m_acqDevice)
        {
            try
            {
                m_acqDevice->Destroy();
                delete m_acqDevice;
            }
            catch (...)
            {
                qWarning() << "Exception when cleaning up acquisition device";
            }
            m_acqDevice = nullptr;
        }

        m_frames.clear();
        m_connFailed = false;
        m_connected = false;
        m_isAcquiring = false;

        qDebug() << "Cleanup completed for:" << m_serverName;
    }
    catch (...)
    {
        qWarning() << "Unhandled exception during cleanup";
        // Reset all pointers to nullptr to prevent double-free attempts
        m_transfer = nullptr;
        m_colorConv = nullptr;
        m_rawBuffer = nullptr;
        m_acqDevice = nullptr;
        m_isAcquiring = false;
        m_connected = false;
    }
}

bool SaperaCameraReal::connectCamera()
{
    try
    {
        // Make sure we clean up any previously created objects first
        cleanup();

        // Initialize Sapera with error suppression - do this FIRST
        SapManager::SetDisplayStatusMode(FALSE);

        qDebug() << "Connecting to camera:" << m_serverName;

        // Create SapLocation directly using the server name from original sample code
        // The key is using the naming pattern Nano-C4020_N as in the console sample
        *m_loc = SapLocation(m_serverName.toStdString().c_str(), 0);

        // Find the configuration file (use the copy in build directory)
        QString ccfPath = QCoreApplication::applicationDirPath() + "/NanoC4020.ccf";
        QFileInfo checkFile(ccfPath);

        // Create the acquisition device - direct approach like sample code
        m_acqDevice = new SapAcqDevice(*m_loc, ccfPath.toStdString().c_str());

        // Create buffer - simpler approach
        m_rawBuffer = new SapBuffer(2, m_acqDevice);

        // Create transfer - simpler approach matching sample
        m_transfer = new SapAcqDeviceToBuf(m_acqDevice, m_rawBuffer, XferCallback, this);

        qDebug() << "Creating Sapera objects...";

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

        // Create color conversion
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

        // Start acquisition
        if (!m_transfer->Grab())
        {
            qWarning() << "Failed to start acquisition";
            cleanup();
            return false;
        }

        m_connected = true;
        m_isAcquiring = true;
        qDebug() << "Camera connected and acquisition started";
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

void SaperaCameraReal::disconnectCamera()
{
    stopAcquisition();
    cleanup();
}

bool SaperaCameraReal::isConnected() const
{
    return m_connected;
}

bool SaperaCameraReal::startAcquisition()
{
    if (!m_transfer)
    {
        qWarning() << "Cannot start acquisition - transfer object is null for camera:" << m_serverName;
        return false;
    }

    return sapera_utils::safe_sapera_op([&]()
                                        {
        bool result = m_transfer->Grab();
        if (result) {
            qDebug() << "Successfully started acquisition for camera:" << m_serverName;
        }
        return result; }, "start acquisition");
}

void SaperaCameraReal::stopAcquisition()
{
    if (!m_transfer)
    {
        return; // Nothing to do
    }

    sapera_utils::safe_sapera_op([&]()
                                 {
        m_transfer->Freeze();
        qDebug() << "Acquisition stopped for camera:" << m_serverName;
        return true; }, "stop acquisition");
}

bool SaperaCameraReal::capturePhoto(const QString &filePath)
{
    qCritical() << "========== CAPTURE PHOTO START ==========";
    qCritical() << "Capturing photo to:" << filePath;

    // Disable error dialogs immediately
    SapManager::SetDisplayStatusMode(FALSE);

    // Track if we're actively taking a photo to prevent concurrent access
    static std::atomic<bool> isCapturingPhoto(false);

    // Check if we're already in a capture operation
    if (isCapturingPhoto.exchange(true))
    {
        qCritical() << "Another photo capture is already in progress, please wait";
        return false;
    }

    // Use RAII pattern to guarantee we reset the capturing flag on exit
    struct CaptureGuard
    {
        CaptureGuard() {}
        ~CaptureGuard()
        {
            isCapturingPhoto.store(false);
            qCritical() << "Photo capture flag reset - safe for new captures";
        }
    } captureGuard;

    // We need a valid connection and active resources
    if (!m_connected || !m_acqDevice || !m_rawBuffer || !m_transfer)
    {
        qCritical() << "Camera not connected or resources not initialized";
        return false;
    }

    // CRITICAL: Make sure we have a valid m_serverName
    if (m_serverName.isEmpty() || m_serverName == "System")
    {
        qCritical() << "Invalid camera name for capture:" << m_serverName;
        return false;
    }

    qCritical() << "Using camera name:" << m_serverName << "for photo capture";

    // Determine save path
    QString savePath = filePath;
    if (savePath.isEmpty())
    {
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
        savePath = QCoreApplication::applicationDirPath() + "/capture_" + timestamp + ".tiff";
    }

    bool saveResult = false;
    bool wasAcquiring = m_isAcquiring;
    // Declare output buffer pointer early - before any goto statements
    SapBuffer *outBuffer = nullptr;
    // Define format options for image saving before any goto statements
    QString formatOptions;

    // Choose format based on file extension
    if (savePath.toLower().endsWith(".png"))
    {
        formatOptions = "-format png -compression 6"; // PNG with moderate compression
    }
    else if (savePath.toLower().endsWith(".jpg") || savePath.toLower().endsWith(".jpeg"))
    {
        formatOptions = "-format jpeg -compression 90"; // JPEG with high quality
    }
    else
    {
        // Default to TIFF with light compression for compatibility
        formatOptions = "-format tiff -compression 1";
    }

    try
    {
        // Add a delay before any operation to ensure stable state
        QThread::msleep(50);

        // Completely stop the acquisition - we MUST do this
        // to avoid "Cannot be called during live grab" error
        if (m_transfer)
        {
            qCritical() << "Stopping acquisition before snapshot";
            // First freeze acquisition
            m_transfer->Freeze();

            // Wait for resources to fully release
            QThread::msleep(300);

            // Abort any pending transfer operations
            m_transfer->Abort();

            // Wait again to ensure all resources are released
            QThread::msleep(200);

            // Reset acquisition state
            m_isAcquiring = false;
        }

        // Take a fresh snapshot with resource conflict handling
        qCritical() << "Taking a fresh snapshot";

        int retryCount = 0;
        bool snapSuccess = false;

        // Attempt snapshot with retries
        while (!snapSuccess && retryCount < 3)
        {
            try
            {
                // Make absolutely sure we're not in grab mode
                if (retryCount > 0)
                {
                    qCritical() << "Retry attempt " << retryCount << " - ensure transfer is reset";
                    m_transfer->Abort();
                    QThread::msleep(500);
                }

                // Now attempt the snapshot
                snapSuccess = m_transfer->Snap();

                if (!snapSuccess)
                {
                    retryCount++;
                    qCritical() << "Snap failed, retrying..." << retryCount;
                    QThread::msleep(300 * retryCount); // Increasing delay with each retry
                }
            }
            catch (const std::exception &e)
            {
                qCritical() << "Exception during Snap operation:" << e.what() << ", retrying..." << retryCount;
                retryCount++;
                QThread::msleep(500 * retryCount);
            }
            catch (...)
            {
                qCritical() << "Unknown exception during Snap operation, retrying..." << retryCount;
                retryCount++;
                QThread::msleep(500 * retryCount);
            }
        }

        if (!snapSuccess)
        {
            qCritical() << "Failed to snap after multiple attempts";
            goto cleanup_and_restart;
        }

        // Wait for the transfer to complete
        qCritical() << "Waiting for transfer to complete";
        if (!m_transfer->Wait(5000)) // Increased timeout to 5 seconds
        {
            qCritical() << "Transfer timeout while capturing frame";
            goto cleanup_and_restart;
        }

        // Now we should have a fresh frame in the buffer
        qCritical() << "Got fresh frame, processing for capture";

        // Add a short delay to stabilize
        QThread::msleep(100);

        // Check if we have a color conversion object
        if (!m_colorConv)
        {
            qCritical() << "Missing color conversion object for capture";
            goto cleanup_and_restart;
        }

        // Use the color conversion on the fresh frame
        if (!m_colorConv->Convert())
        {
            qCritical() << "Failed to convert buffer for capture";
            goto cleanup_and_restart;
        }

        // Get the output buffer - assign to the previously declared pointer
        outBuffer = m_colorConv->GetOutputBuffer();
        if (!outBuffer)
        {
            qCritical() << "Failed to get output buffer";
            goto cleanup_and_restart;
        }

        // Save the image
        qCritical() << "Saving image to:" << savePath << "using format:" << formatOptions;

        // Use the memory-efficient format options defined earlier
        try
        {
            // Make sure we have a valid buffer
            if (!outBuffer)
            {
                qCritical() << "Save error: output buffer is null";
                goto cleanup_and_restart;
            }

            // Make sure dimensions are reasonable
            if (outBuffer->GetWidth() <= 0 || outBuffer->GetHeight() <= 0)
            {
                qCritical() << "Save error: invalid buffer dimensions";
                goto cleanup_and_restart;
            }

            // Save with explicit error handling
            saveResult = outBuffer->Save(savePath.toStdString().c_str(), formatOptions.toStdString().c_str());

            if (saveResult)
            {
                qCritical() << "Image saved successfully";
                // Also update and emit the last frame
                QImage image = SapBufferToQImage(outBuffer);
                if (!image.isNull())
                {
                    m_lastFrame = image;
                    emit frameReady(image);
                }
            }
            else
            {
                qCritical() << "Failed to save image";
            }
        }
        catch (const std::exception &e)
        {
            qCritical() << "Exception during image save:" << e.what();
            saveResult = false;
        }
        catch (...)
        {
            qCritical() << "Unknown exception during image save";
            saveResult = false;
        }

        // Wait before restarting acquisition to ensure memory is freed
        QThread::msleep(400);

    cleanup_and_restart:
        // Always restart acquisition if it was running before
        if (wasAcquiring && m_transfer)
        {
            qCritical() << "Restarting live acquisition";
            // Wait to make sure resources are freed
            QThread::msleep(200);

            try
            {
                // Initialize the transfer object first
                if (m_transfer->Init())
                {
                    // Start grabbing frames
                    bool result = m_transfer->Grab();
                    m_isAcquiring = result;
                    qCritical() << "Acquisition restart " << (result ? "successful" : "failed");
                }
                else
                {
                    qCritical() << "Failed to re-initialize transfer before restart";
                }
            }
            catch (const std::exception &e)
            {
                qCritical() << "Exception while restarting acquisition: " << e.what();
            }
            catch (...)
            {
                qCritical() << "Unknown exception while restarting acquisition";
            }
        }

        qCritical() << "========== CAPTURE PHOTO " << (saveResult ? "SUCCESS" : "FAILED") << " ==========";
        return saveResult;
    }
    catch (const std::exception &e)
    {
        qCritical() << "Exception in capturePhoto:" << e.what();

        // Make sure we restart acquisition in case of exception
        if (wasAcquiring && m_transfer)
        {
            try
            {
                // Add a delay before trying to restart
                QThread::msleep(300);

                if (m_transfer->Init())
                {
                    bool result = m_transfer->Grab();
                    m_isAcquiring = result;
                }
            }
            catch (...)
            {
                qCritical() << "Failed to restart acquisition after exception";
            }
        }

        return false;
    }
    catch (...)
    {
        qCritical() << "Unknown exception in capturePhoto";

        // Make sure we restart acquisition in case of exception
        if (wasAcquiring && m_transfer)
        {
            try
            {
                // Add a delay before trying to restart
                QThread::msleep(300);

                if (m_transfer->Init())
                {
                    bool result = m_transfer->Grab();
                    m_isAcquiring = result;
                }
            }
            catch (...)
            {
                qCritical() << "Failed to restart acquisition after exception";
            }
        }

        return false;
    }
}

QImage SaperaCameraReal::getLatestFrame()
{
    return m_lastFrame;
}

// More robust static callback function that handles potential NULL contexts properly
void SaperaCameraReal::XferCallback(SapXferCallbackInfo *pInfo)
{
    // Disable Sapera error dialogs during callback
    SapManager::SetDisplayStatusMode(FALSE);

    if (!pInfo)
    {
        qWarning() << "XferCallback received null info parameter";
        return;
    }

    // Get context safely
    SaperaCameraReal *self = nullptr;
    try
    {
        self = static_cast<SaperaCameraReal *>(pInfo->GetContext());
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
        // First validate objects
        if (!self->m_rawBuffer) {
            qWarning() << "Raw buffer is null for camera:" << self->getServerName();
            return false;
        }
        
        // Process with color conversion if available
        if (self->m_colorConv) {
            // Safely try color conversion
            bool conversionSuccess = sapera_utils::safe_sapera_op([&]() {
                return self->m_colorConv->Convert();
            }, "color conversion");
            
            if (conversionSuccess) {
                // Safely get the output buffer
                SapBuffer* outputBuffer = self->m_colorConv->GetOutputBuffer();
                if (outputBuffer) {
                    // Validate buffer before conversion
                    void* testAddr = nullptr;
                    bool validBuffer = outputBuffer->GetAddress(&testAddr) && testAddr != nullptr;
                    
                    if (validBuffer) {
                        QImage img = SapBufferToQImage(outputBuffer);
                        if (!img.isNull()) {
                            self->m_lastFrame = img;
                            emit self->frameReady(img);
                            return true;
                        } else {
                            qWarning() << "SapBufferToQImage returned null image for camera:" << self->getServerName();
                        }
                    } else {
                        qWarning() << "Output buffer has invalid address for camera:" << self->getServerName();
                    }
                } else {
                    qWarning() << "Null output buffer from color conversion for camera:" << self->getServerName();
                }
            } else {
                qWarning() << "Color conversion failed for camera:" << self->getServerName();
            }
        }
        
        // If we get here, either no color conversion or it failed
        // Create a fallback image
        int width = 0, height = 0;
        sapera_utils::safe_sapera_op([&]() {
            width = self->m_rawBuffer->GetWidth();
            height = self->m_rawBuffer->GetHeight();
            return (width > 0 && height > 0);
        }, "get buffer dimensions");
        
        if (width > 0 && height > 0) {
            QImage fallbackImg(width, height, QImage::Format_RGB888);
            fallbackImg.fill(Qt::darkGray);
            self->m_lastFrame = fallbackImg;
            emit self->frameReady(fallbackImg);
            return true;
        }
        
        return false; }, "process frame");
}

QString SaperaCameraReal::getServerName() const
{
    return m_serverName;
}

std::vector<QString> SaperaCameraReal::enumerateCameras()
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