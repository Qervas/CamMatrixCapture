#pragma once
#include <SapClassBasic.h>
#include <QObject>
#include <QImage>
#include <QString>
#include <vector>
#include <string>

class SaperaCameraReal : public QObject {
    Q_OBJECT
public:
    SaperaCameraReal(int serverIndex, const QString& serverName, const QString& configName, QObject* parent = nullptr);
    ~SaperaCameraReal();

    bool connectCamera();
    void disconnectCamera();
    bool startAcquisition();
    void stopAcquisition();
    bool capturePhoto(const QString& filePath);
    QImage getLatestFrame();
    QString getServerName() const;
    bool isConnected() const;

    static std::vector<QString> enumerateCameras();

signals:
    void frameReady(const QImage&);

private:
    void cleanup();
    static void XferCallback(SapXferCallbackInfo* pInfo);

    int m_serverIndex;
    QString m_serverName;
    QString m_configName;
    bool m_connected = false;

    SapLocation* m_loc = nullptr;
    SapAcqDevice* m_acqDevice = nullptr;
    SapBuffer* m_rawBuffer = nullptr;
    SapBuffer* m_rgbBuffer = nullptr;
    SapColorConversion* m_colorConv = nullptr;
    SapAcqDeviceToBuf* m_transfer = nullptr;

    QImage m_lastFrame;
}; 