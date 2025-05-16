#include <QObject>
#include <QImage>
#include <QString>
#include "sapera_camera_real.hpp"
#include <QDebug>
#include <QBuffer>

static QImage SapBufferToQImage(SapBuffer* buffer) {
    if (!buffer) return QImage();
    int width = buffer->GetWidth();
    int height = buffer->GetHeight();
    int format = buffer->GetFormat();
    void* pData = nullptr;
    if (!buffer->GetAddress(&pData) || !pData) return QImage();
    if (format == SapFormatRGB888) {
        return QImage(static_cast<const uchar*>(pData), width, height, width * 3, QImage::Format_RGB888).copy();
    }
    return QImage();
}

SaperaCameraReal::SaperaCameraReal(int serverIndex, const QString& serverName, const QString& configName, QObject* parent)
    : QObject(parent), m_serverIndex(serverIndex), m_serverName(serverName), m_configName(configName)
{
    m_loc = new SapLocation(serverIndex, 0);
}

SaperaCameraReal::~SaperaCameraReal() {
    cleanup();
    delete m_loc;
}

void SaperaCameraReal::cleanup() {
    if (m_transfer) { m_transfer->Destroy(); delete m_transfer; m_transfer = nullptr; }
    if (m_colorConv) { m_colorConv->Destroy(); delete m_colorConv; m_colorConv = nullptr; }
    if (m_rgbBuffer) { m_rgbBuffer->Destroy(); delete m_rgbBuffer; m_rgbBuffer = nullptr; }
    if (m_rawBuffer) { m_rawBuffer->Destroy(); delete m_rawBuffer; m_rawBuffer = nullptr; }
    if (m_acqDevice) { m_acqDevice->Destroy(); delete m_acqDevice; m_acqDevice = nullptr; }
    m_connected = false;
}

bool SaperaCameraReal::connectCamera() {
    cleanup();
    m_acqDevice = new SapAcqDevice(*m_loc, m_configName.toStdString().c_str());
    if (!m_acqDevice->Create()) {
        qWarning() << "Failed to create SapAcqDevice for server:" << m_serverName;
        delete m_acqDevice; m_acqDevice = nullptr;
        m_connected = false;
        return false;
    }
    m_rawBuffer = new SapBuffer(4, m_acqDevice);
    if (!m_rawBuffer->Create()) {
        qWarning() << "Failed to create raw buffer for server:" << m_serverName;
        cleanup();
        return false;
    }
    m_colorConv = new SapColorConversion(m_acqDevice, m_rawBuffer);
    if (!m_colorConv->Create()) {
        qWarning() << "Failed to create color conversion for server:" << m_serverName;
        cleanup();
        return false;
    }

    // Attempt to enable software color conversion
    if (!m_colorConv->Enable(TRUE, FALSE)) { // TRUE to enable, FALSE for useHardware (i.e., prefer software)
        qWarning() << "Failed to enable software color conversion for server:" << m_serverName;
        // Check if it's because software isn't supported at all
        if (!m_colorConv->IsSoftwareSupported(TRUE)) { // TRUE to display error message from SDK
             qWarning() << "Software conversion is not supported by the SDK/system for this setup.";
        }
        cleanup();
        return false;
    }
    qDebug() << "Software color conversion enabled for:" << m_serverName;

    m_colorConv->SetOutputFormat(SapFormatRGB888);

    // Dynamically get alignment from the acquisition device
    SapColorConversion::Align detectedAlign = SapColorConversion::GetAlignModeFromAcqDevice(m_acqDevice);
    qDebug() << "Detected Bayer alignment for" << m_serverName << ":" << detectedAlign;
    
    // Check if a valid alignment was detected. If not, AlignNone (0) might be returned.
    // Depending on the camera, if it's not Bayer, these Bayer-specific settings might not be needed
    // or could cause errors if set with AlignNone.
    // For now, we'll proceed with the detected alignment.
    if (!m_colorConv->SetAlign(detectedAlign)) {
        qWarning() << "Failed to set Bayer alignment" << detectedAlign << "for camera:" << m_serverName;
        // Optionally, decide if this is a fatal error for connection
        // cleanup(); 
        // return false;
    }
    
    // Set method (assuming Method1 is generally applicable for Bayer to RGB)
    if (!m_colorConv->SetMethod(SapColorConversion::Method1)) {
        qWarning() << "Failed to set color conversion method 1 for camera:" << m_serverName;
        // Optionally, decide if this is a fatal error
        // cleanup();
        // return false;
    }

    m_transfer = new SapAcqDeviceToBuf(m_acqDevice, m_rawBuffer);
    m_transfer->SetCallbackInfo(XferCallback, this);
    m_transfer->SetAutoEmpty(true);

    if (!m_transfer->Create()) {
        qWarning() << "Failed to create transfer for server:" << m_serverName;
        cleanup();
        return false;
    }

    if (!this->startAcquisition()) {
        qWarning() << "Failed to start acquisition for server:" << m_serverName;
        cleanup(); // This will set m_connected to false
        return false;
    }

    m_connected = true; // Successfully connected and acquisition started
    qDebug() << "Camera connected and acquisition started for:" << m_serverName;
    return true;
}

void SaperaCameraReal::disconnectCamera() {
    stopAcquisition();
    cleanup();
}

bool SaperaCameraReal::isConnected() const {
    return m_connected;
}

bool SaperaCameraReal::startAcquisition() {
    if (!m_transfer) return false;
    return m_transfer->Grab();
}

void SaperaCameraReal::stopAcquisition() {
    if (m_transfer) m_transfer->Freeze();
}

bool SaperaCameraReal::capturePhoto(const QString& filePath) {
    if (!m_colorConv) return false;
    m_transfer->Freeze();
    m_transfer->Wait(5000);
    if (m_colorConv->Convert()) {
        return m_colorConv->GetOutputBuffer()->Save(filePath.toStdString().c_str(), "bmp");
    }
    return false;
}

QImage SaperaCameraReal::getLatestFrame() {
    return m_lastFrame;
}

void SaperaCameraReal::XferCallback(SapXferCallbackInfo* pInfo) {
    SaperaCameraReal* self = reinterpret_cast<SaperaCameraReal*>(pInfo->GetContext());
    if (!self) return;

    qDebug() << "XferCallback triggered for camera:" << self->getServerName();

    // Always process the frame
    if (self->m_colorConv && self->m_colorConv->Convert()) {
        QImage img = SapBufferToQImage(self->m_colorConv->GetOutputBuffer());
        qDebug() << "Frame converted to QImage for" << self->getServerName() << ", valid:" << !img.isNull() << "size:" << img.size();
        self->m_lastFrame = img;
        emit self->frameReady(img);
        qDebug() << "Emitted frameReady for camera:" << self->getServerName();
    } else {
        qDebug() << "Color conversion failed or m_colorConv is null for camera:" << self->getServerName();
    }
}

QString SaperaCameraReal::getServerName() const {
    return m_serverName;
}

std::vector<QString> SaperaCameraReal::enumerateCameras() {
    std::vector<QString> result;
    int count = SapManager::GetServerCount();
    for (int i = 0; i < count; ++i) {
        char name[256];
        if (SapManager::GetServerName(i, name, sizeof(name)))
            result.push_back(QString::fromUtf8(name));
    }
    return result;
} 