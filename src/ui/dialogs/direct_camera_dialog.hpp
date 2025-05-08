#pragma once

#include <QDialog>
#include <QPushButton>
#include <QStatusBar>
#include <vector>
#include <string>

namespace cam_matrix::ui {

class DirectCameraWidget;

class DirectCameraDialog : public QDialog {
    Q_OBJECT

public:
    explicit DirectCameraDialog(QWidget* parent = nullptr);
    ~DirectCameraDialog() override = default;

public slots:
    void refreshCameras();

signals:
    void camerasFound(const std::vector<std::string>& cameraNames);
    void cameraDetected(const std::string& cameraName);
    void refreshMainCameraView();

private slots:
    void onStatusChanged(const QString& status);
    void onError(const QString& error);

private:
    DirectCameraWidget* cameraWidget_;
    QPushButton* closeButton_;
    QStatusBar* statusBar_;
};

} // namespace cam_matrix::ui 