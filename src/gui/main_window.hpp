#pragma once

#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QStatusBar>

namespace cam_matrix {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onCaptureButtonClicked();
    void onSettingsButtonClicked();

private:
    void setupUi();
    void createConnections();

    // UI elements
    QWidget* centralWidget;
    QVBoxLayout* mainLayout;
    QPushButton* captureButton;
    QPushButton* settingsButton;
    QLabel* statusLabel;
    QStatusBar* statusBar;
};

} // namespace cam_matrix 