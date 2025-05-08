#pragma once

#include <QWidget>
#include <QGroupBox>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>

namespace cam_matrix::ui {

class CameraControlWidget : public QWidget {
    Q_OBJECT

public:
    explicit CameraControlWidget(QWidget* parent = nullptr);
    ~CameraControlWidget() = default;

    void setCameraIndex(int index);

    void setExposure(double value);
    void setGain(double value);
    void setFormat(const QString& format);
    
    // Override the setEnabled method to ensure proper button state
    void setEnabled(bool enabled);

signals:
    void controlValueChanged(const QString& controlName, int value);
    void statusChanged(const QString& message);
    void capturePhotoRequested(int cameraIndex);
    
    void exposureChanged(double value);
    void gainChanged(double value);
    void formatChanged(const QString& format);
    void photoCaptureRequested();

private slots:
    void onExposureChanged(int value);
    void onGainChanged(int value);
    void onFormatChanged(int index);
    void onCapturePhotoClicked();

private:
    void setupUi();
    void createConnections();

    QGroupBox* exposureGroup_;
    QSlider* exposureSlider_;

    QGroupBox* gainGroup_;
    QSlider* gainSlider_;

    QGroupBox* formatGroup_;
    QComboBox* formatCombo_;

    QCheckBox* autoExposureCheck_;
    
    QGroupBox* captureGroup_;
    QPushButton* captureButton_;

    int currentCameraIndex_;
};

} // namespace cam_matrix::ui
