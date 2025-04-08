#pragma once
#include <QDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include "core/camera_tester.hpp"

namespace cam_matrix::ui {

class CameraTestDialog : public QDialog {
    Q_OBJECT

public:
    explicit CameraTestDialog(QWidget* parent = nullptr);
    ~CameraTestDialog() override = default;

private:
    void setupUi();

    core::CameraTester* tester_;
    QPushButton* closeButton_;
};

} // namespace cam_matrix::ui
