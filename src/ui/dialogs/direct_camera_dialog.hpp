#pragma once

#include <QDialog>
#include <QPushButton>
#include <QStatusBar>

namespace cam_matrix::ui {

class DirectCameraWidget;

class DirectCameraDialog : public QDialog {
    Q_OBJECT

public:
    explicit DirectCameraDialog(QWidget* parent = nullptr);
    ~DirectCameraDialog() override = default;

private slots:
    void onStatusChanged(const QString& status);
    void onError(const QString& error);

private:
    DirectCameraWidget* cameraWidget_;
    QPushButton* closeButton_;
    QStatusBar* statusBar_;
};

} // namespace cam_matrix::ui 