#pragma once

#include <QDialog>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QString>

namespace cam_matrix::ui {

class CameraModeSelectorDialog : public QDialog {
    Q_OBJECT

public:
    explicit CameraModeSelectorDialog(QWidget* parent = nullptr);
    ~CameraModeSelectorDialog() override = default;

    QString getSelectedMode() const;

private:
    void setupUi();
    void populateModeComboBox();

    QComboBox* modeComboBox_;
    QPushButton* okButton_;
    QPushButton* cancelButton_;
};

} // namespace cam_matrix::ui 