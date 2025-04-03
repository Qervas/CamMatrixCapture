#pragma once

#include "ui/pages/page.hpp"
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QGraphicsView>
#include <memory>

namespace cam_matrix::ui {

class CapturePreviewWidget;

class CapturePage : public Page {
    Q_OBJECT

public:
    explicit CapturePage(QWidget* parent = nullptr);
    ~CapturePage() override;

    QString title() const override { return tr("Capture"); }
    void initialize() override;
    void cleanup() override;

private slots:
    void onStartCapture();
    void onStopCapture();
    void updateProgress(int value);

private:
    void setupUi() override;
    void createConnections() override;

    QPushButton* startButton_;
    QPushButton* stopButton_;
    QProgressBar* progressBar_;
    QGraphicsView* previewWidget_;
};
} // namespace cam_matrix::ui
