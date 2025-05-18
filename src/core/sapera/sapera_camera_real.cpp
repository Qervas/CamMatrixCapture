#include <QObject>
#include <QImage>
#include <QString>
#include "sapera_camera_real.hpp"
#include "sapera_utils.hpp"
#include <QDebug>
#include <QBuffer>
#include <functional>

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
    // Disable error dialogs during cleanup
    SapManager::SetDisplayStatusMode(FALSE);
    
    try {
        // Stop and clean up all resources - order matters!
        if (m_transfer) {
            try {
                if (m_isAcquiring) {
                    try {
                        // First try to stop acquisition if it's running
                        m_transfer->Freeze();
                        m_isAcquiring = false;
                        qDebug() << "Stopped acquisition for:" << m_serverName;
                    } catch (...) {
                        qWarning() << "Exception when stopping acquisition - continuing cleanup";
                    }
                }
                
                try {
                    m_transfer->Destroy();
                } catch (...) {
                    qWarning() << "Exception when destroying transfer object - continuing cleanup";
                }
                
                delete m_transfer;
            } catch (...) {
                qWarning() << "Exception when deleting transfer object";
            }
            m_transfer = nullptr;
        }

        if (m_colorConv) {
            try {
                m_colorConv->Destroy();
                delete m_colorConv;
            } catch (...) {
                qWarning() << "Exception when cleaning up color conversion object";
            }
            m_colorConv = nullptr;
        }

        if (m_rawBuffer) {
            try {
                m_rawBuffer->Destroy();
                delete m_rawBuffer;
            } catch (...) {
                qWarning() << "Exception when cleaning up buffer object";
            }
            m_rawBuffer = nullptr;
        }

        if (m_acqDevice) {
            try {
                m_acqDevice->Destroy();
                delete m_acqDevice;
            } catch (...) {
                qWarning() << "Exception when cleaning up acquisition device";
            }
            m_acqDevice = nullptr;
        }

        m_frames.clear();
        m_connFailed = false;
        m_connected = false;
        m_isAcquiring = false;
        
        qDebug() << "Cleanup completed for:" << m_serverName;
    } catch (...) {
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

bool SaperaCameraReal::connectCamera() {
    try {
        // Make sure we clean up any previously created objects first
        cleanup();
        
        // Initialize Sapera with error suppression - do this FIRST
        sapera_utils::initialize_sapera();
        
        qDebug() << "Connecting to camera:" << m_serverName;
        
        // Use unique_ptr for RAII during connection process
        // Important: Use the server name directly as in the working version
        std::unique_ptr<SapAcqDevice> tempAcqDevice;
        try {
            tempAcqDevice.reset(new SapAcqDevice(m_serverName.toStdString().c_str()));
        } catch (const std::exception& e) {
            qWarning() << "Exception creating acquisition device:" << e.what();
            m_connected = false;
            return false;
        } catch (...) {
            qWarning() << "Unknown exception creating acquisition device";
            m_connected = false;
            return false;
        }
        
        if (!tempAcqDevice) {
            qWarning() << "Failed to allocate acquisition device for server:" << m_serverName;
            m_connected = false;
            return false;
        }
        
        // Set display mode on the device to suppress errors
        try {
            tempAcqDevice->SetDisplayStatusMode(FALSE);
        } catch (...) {
            qWarning() << "Exception while setting display mode";
            // Continue anyway
        }
        
        // Use SFINAE for creating the device, but maintain the working approach
        if (!sapera_utils::safe_create(tempAcqDevice.get(), "acquisition device")) {
            qWarning() << "Failed to create acquisition device for server:" << m_serverName;
            m_connected = false;
            return false;
        }
        
        qDebug() << "Successfully created acquisition device for:" << m_serverName;
        
        // Create buffer with SFINAE utility (2 buffers for continuous acquisition)
        std::unique_ptr<SapBuffer> tempRawBuffer;
        try {
            tempRawBuffer.reset(new SapBuffer(2, tempAcqDevice.get()));
            if (!tempRawBuffer) {
                qWarning() << "Failed to allocate buffer for server:" << m_serverName;
                return false;
            }
        } catch (...) {
            qWarning() << "Exception creating buffer for server:" << m_serverName;
            return false;
        }
        
        // Create the buffer using SFINAE
        if (!sapera_utils::safe_create(tempRawBuffer.get(), "raw buffer")) {
            qWarning() << "Failed to create raw buffer for server:" << m_serverName;
            return false;
        }
        
        // Get the buffer format safely
        SapFormat rawBufferFormatVal = 0; // Default to 0 (usually means unknown/invalid)
        try {
            rawBufferFormatVal = tempRawBuffer->GetFormat();
            qDebug() << "Raw buffer format for" << m_serverName << "is:" << static_cast<int>(rawBufferFormatVal);
        } catch (...) {
            qWarning() << "Exception getting buffer format - continuing anyway";
        }
        
        // Attempt to create color conversion - but don't fail if it doesn't work
        std::unique_ptr<SapColorConversion> tempColorConv;
        bool colorConvCreated = false;
        try {
            tempColorConv.reset(new SapColorConversion(tempAcqDevice.get(), tempRawBuffer.get()));
            if (tempColorConv && sapera_utils::safe_create(tempColorConv.get(), "color conversion")) {
                colorConvCreated = true;
                qDebug() << "Created color conversion for server:" << m_serverName;
            } else {
                qWarning() << "Could not create color conversion for server:" << m_serverName;
            }
        } catch (...) {
            qWarning() << "Exception creating color conversion - continuing without it";
        }
        
        // Only configure color conversion if created successfully
        bool colorEnabled = false;
        if (colorConvCreated) {
            try {
                // Use the safe operation wrapper for enabling
                colorEnabled = sapera_utils::safe_sapera_op(
                    [&]() { return tempColorConv->Enable(TRUE, FALSE); },
                    "enable color conversion"
                );
                
                if (colorEnabled) {
                    qDebug() << "Color conversion enabled for:" << m_serverName;
                    
                    // Set format now that conversion is enabled
                    sapera_utils::safe_sapera_op(
                        [&]() { return tempColorConv->SetOutputFormat(SapFormatRGB888); },
                        "set output format"
                    );
                    
                    // Get alignment data safely
                    SapColorConversion::Align detectedAlign = SapColorConversion::AlignNone;
                    sapera_utils::safe_sapera_op(
                        [&]() {
                            detectedAlign = SapColorConversion::GetAlignModeFromAcqDevice(tempAcqDevice.get());
                            return true;
                        },
                        "get Bayer alignment"
                    );
                    
                    qDebug() << "Detected Bayer alignment for" << m_serverName << "is:" << detectedAlign;
                    
                    // Set alignment if valid
                    if (detectedAlign != SapColorConversion::AlignNone) {
                        sapera_utils::safe_sapera_op(
                            [&]() { return tempColorConv->SetAlign(detectedAlign); },
                            "set Bayer alignment"
                        );
                        
                        sapera_utils::safe_sapera_op(
                            [&]() { return tempColorConv->SetMethod(SapColorConversion::Method1); },
                            "set conversion method"
                        );
                    }
                } else {
                    qWarning() << "Color conversion could not be enabled for:" << m_serverName;
                }
            } catch (...) {
                qWarning() << "Exception configuring color conversion - continuing without it";
                colorConvCreated = false;
            }
        }
        
        // Create transfer object with SFINAE utility
        std::unique_ptr<SapAcqDeviceToBuf> tempTransfer;
        try {
            tempTransfer.reset(new SapAcqDeviceToBuf(tempAcqDevice.get(), tempRawBuffer.get()));
            if (!tempTransfer) {
                qWarning() << "Failed to allocate transfer object for:" << m_serverName;
                return false;
            }
            
            tempTransfer->SetCallbackInfo(XferCallback, this);
            tempTransfer->SetAutoEmpty(true);
        } catch (...) {
            qWarning() << "Exception creating transfer object for:" << m_serverName;
            return false;
        }
        
        qDebug() << "Creating transfer object for:" << m_serverName;
        try {
            if (!sapera_utils::safe_create(tempTransfer.get(), "transfer")) {
                qWarning() << "Failed to create transfer object for:" << m_serverName;
                return false;
            }
        } catch (...) {
            qWarning() << "Exception in transfer creation for:" << m_serverName;
            return false;
        }
        
        qDebug() << "Starting acquisition for:" << m_serverName;
        // Start acquisition using safe wrapper with extra protection
        bool acquisitionStarted = false;
        try {
            acquisitionStarted = sapera_utils::safe_sapera_op(
                [&]() { return tempTransfer->Grab(); },
                "start acquisition"
            );
            
            if (!acquisitionStarted) {
                qWarning() << "Failed to start acquisition for:" << m_serverName;
                return false;
            }
        } catch (...) {
            qWarning() << "Exception starting acquisition for:" << m_serverName;
            return false;
        }
        
        // Transfer ownership of the temp objects to the member variables
        try {
            m_acqDevice = tempAcqDevice.release();
            m_rawBuffer = tempRawBuffer.release();
            if (colorConvCreated) {
                m_colorConv = tempColorConv.release();
            }
            m_transfer = tempTransfer.release();
        } catch (...) {
            qWarning() << "Exception during object ownership transfer";
            cleanup(); // Make sure we clean up any partially initialized objects
            return false;
        }
    
        m_connected = true;
        qDebug() << "Camera connected and acquisition started for:" << m_serverName;
        return true;
    } catch (const std::exception& e) {
        qWarning() << "Exception in connectCamera:" << e.what();
        cleanup();
        m_connected = false;
        return false;
    } catch (...) {
        qWarning() << "Unknown exception in connectCamera";
        cleanup();
        m_connected = false;
        return false;
    }
}

void SaperaCameraReal::disconnectCamera() {
    stopAcquisition();
    cleanup();
}

bool SaperaCameraReal::isConnected() const {
    return m_connected;
}

bool SaperaCameraReal::startAcquisition() {
    if (!m_transfer) {
        qWarning() << "Cannot start acquisition - transfer object is null for camera:" << m_serverName;
        return false;
    }
    
    return sapera_utils::safe_sapera_op([&]() {
        bool result = m_transfer->Grab();
        if (result) {
            qDebug() << "Successfully started acquisition for camera:" << m_serverName;
        }
        return result;
    }, "start acquisition");
}

void SaperaCameraReal::stopAcquisition() {
    if (!m_transfer) {
        return; // Nothing to do
    }
    
    sapera_utils::safe_sapera_op([&]() {
        m_transfer->Freeze();
        qDebug() << "Acquisition stopped for camera:" << m_serverName;
        return true;
    }, "stop acquisition");
}

bool SaperaCameraReal::capturePhoto(const QString& filePath) {
    // Check all required objects
    if (!m_colorConv || !m_transfer) {
        qWarning() << "Cannot capture photo - required objects not initialized for camera:" << m_serverName;
        return false;
    }
    
    return sapera_utils::safe_sapera_op([&]() {
        // First freeze the acquisition
        if (!sapera_utils::safe_sapera_op([&]() {
            m_transfer->Freeze();
            return true;
        }, "freeze for photo")) {
            return false;
        }
        
        // Wait for transfer completion with timeout
        bool waitResult = false;
        sapera_utils::safe_sapera_op([&]() {
            waitResult = m_transfer->Wait(5000);
            return true;
        }, "wait for transfer");
        
        if (!waitResult) {
            qWarning() << "Timeout waiting for transfer completion for camera:" << m_serverName;
            // Restart acquisition and return failure
            sapera_utils::safe_sapera_op([&]() { 
                m_transfer->Grab(); 
                return true; 
            }, "restart after timeout");
            return false;
        }
        
        // Convert the frame with color conversion
        bool conversionResult = sapera_utils::safe_sapera_op([&]() {
            return m_colorConv->Convert();
        }, "convert for photo");
        
        if (!conversionResult) {
            // Restart acquisition and return failure
            sapera_utils::safe_sapera_op([&]() { 
                m_transfer->Grab(); 
                return true; 
            }, "restart after failed conversion");
            return false;
        }
        
        // Get the output buffer
        SapBuffer* outputBuffer = m_colorConv->GetOutputBuffer();
        if (!outputBuffer) {
            qWarning() << "Failed to get output buffer for camera:" << m_serverName;
            // Restart acquisition and return failure
            sapera_utils::safe_sapera_op([&]() { 
                m_transfer->Grab(); 
                return true; 
            }, "restart after null buffer");
            return false;
        }
        
        // Save the image
        bool saveResult = sapera_utils::safe_sapera_op([&]() {
            return outputBuffer->Save(filePath.toStdString().c_str(), "bmp");
        }, "save photo");
        
        if (saveResult) {
            qDebug() << "Successfully saved photo to:" << filePath;
        } else {
            qWarning() << "Failed to save image to:" << filePath;
        }
        
        // Always restart acquisition before returning
        sapera_utils::safe_sapera_op([&]() { 
            m_transfer->Grab(); 
            return true; 
        }, "restart after photo");
        
        return saveResult;
    }, "capture photo operation");
}

QImage SaperaCameraReal::getLatestFrame() {
    return m_lastFrame;
}

void SaperaCameraReal::XferCallback(SapXferCallbackInfo* pInfo) {
    // Disable Sapera error dialogs during callback
    SapManager::SetDisplayStatusMode(FALSE);
    
    // Get context safely
    SaperaCameraReal* self = nullptr;
    sapera_utils::safe_sapera_op([&]() {
        self = static_cast<SaperaCameraReal*>(pInfo->GetContext());
        return (self != nullptr);
    }, "get callback context");
    
    if (!self) {
        qWarning() << "XferCallback received null context";
        return;
    }
    
    // Process frame with safe operations
    sapera_utils::safe_sapera_op([&]() {
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
                    QImage img = SapBufferToQImage(outputBuffer);
                    if (!img.isNull()) {
                        self->m_lastFrame = img;
                        emit self->frameReady(img);
                        return true;
                    }
                }
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
        
        return false;
    }, "process frame");
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