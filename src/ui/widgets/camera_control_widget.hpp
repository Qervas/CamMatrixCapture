#pragma once

#include <QWidget>
#include <QGroupBox>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>

namespace cam_matrix::ui
{

    class CameraControlWidget : public QWidget
    {
        Q_OBJECT

    public:
        explicit CameraControlWidget(QWidget *parent = nullptr);
        ~CameraControlWidget() = default;

        void setCameraIndex(int index);
        void setFormat(const QString &format);

        // Override the setEnabled method to ensure proper button state
        void setEnabled(bool enabled);

    signals:
        void controlValueChanged(const QString &controlName, int value);
        void statusChanged(const QString &message);
        void capturePhotoRequested(int cameraIndex);
        void formatChanged(const QString &format);
        void photoCaptureRequested();
        void highQualityPhotoCaptureRequested(const QString &format);

    private slots:
        void onFormatChanged(int index);
        void onCapturePhotoClicked();
        void onHighQualityCaptureClicked();

    private:
        void setupUi();
        void createConnections();

        QGroupBox *formatGroup_;
        QComboBox *formatCombo_;

        QGroupBox *captureGroup_;
        QPushButton *captureButton_;
        QPushButton *hqCaptureButton_;
        QCheckBox *highQualityCheckbox_;

        int currentCameraIndex_;
        QString currentFormat_;
    };

} // namespace cam_matrix::ui
