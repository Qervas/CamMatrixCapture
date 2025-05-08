#pragma once

#include <QWidget>
#include <QGroupBox>
#include <QComboBox>
#include <QPushButton>

namespace cam_matrix::ui {

class CameraControlWidget : public QWidget {
    Q_OBJECT

public:
    explicit CameraControlWidget(QWidget* parent = nullptr);
    ~CameraControlWidget() = default;

    void setCameraIndex(int index);
    void setFormat(const QString& format);
    
    // Override the setEnabled method to ensure proper button state
    void setEnabled(bool enabled);

signals:
    void controlValueChanged(const QString& controlName, int value);
    void statusChanged(const QString& message);
    void capturePhotoRequested(int cameraIndex);
    void formatChanged(const QString& format);
    void photoCaptureRequested();

private slots:
    void onFormatChanged(int index);
    void onCapturePhotoClicked();

private:
    void setupUi();
    void createConnections();

    QGroupBox* formatGroup_;
    QComboBox* formatCombo_;
    
    QGroupBox* captureGroup_;
    QPushButton* captureButton_;

    int currentCameraIndex_;
};

} // namespace cam_matrix::ui
