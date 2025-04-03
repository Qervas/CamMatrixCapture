#include "capture_page.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>

namespace cam_matrix::ui {

CapturePage::CapturePage(QWidget* parent)
    : Page(parent)
    , startButton_(nullptr)
    , stopButton_(nullptr)
    , progressBar_(nullptr)
    , previewWidget_(nullptr)
{
}

CapturePage::~CapturePage() = default;

void CapturePage::setupUi()
{
    auto* layout = new QVBoxLayout(this);

    // Control buttons
    auto* buttonLayout = new QHBoxLayout;
    startButton_ = new QPushButton(tr("Start Capture"), this);
    stopButton_ = new QPushButton(tr("Stop Capture"), this);
    stopButton_->setEnabled(false);
    buttonLayout->addWidget(startButton_);
    buttonLayout->addWidget(stopButton_);
    layout->addLayout(buttonLayout);

    // Progress bar
    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 100);
    layout->addWidget(progressBar_);

    // Preview (dummy for now)
    previewWidget_ = new QGraphicsView(this);
    previewWidget_->setMinimumSize(640, 480);
    previewWidget_->setStyleSheet("background-color: #222;");
    layout->addWidget(previewWidget_);

    layout->addStretch();
}

void CapturePage::createConnections()
{
    connect(startButton_, &QPushButton::clicked,
            this, &CapturePage::onStartCapture);
    connect(stopButton_, &QPushButton::clicked,
            this, &CapturePage::onStopCapture);
}

void CapturePage::onStartCapture()
{
    startButton_->setEnabled(false);
    stopButton_->setEnabled(true);
    progressBar_->setValue(0);
    emit statusChanged(tr("Starting capture..."));

    // TODO: Implement actual capture
    // For now, just simulate progress
    for(int i = 0; i <= 100; i += 10) {
        QTimer::singleShot(i * 50, this, [this, i]() { updateProgress(i); });
    }
}

void CapturePage::onStopCapture()
{
    startButton_->setEnabled(true);
    stopButton_->setEnabled(false);
    progressBar_->setValue(0);
    emit statusChanged(tr("Capture stopped"));
}

void CapturePage::updateProgress(int value)
{
    progressBar_->setValue(value);
    if (value >= 100) {
        emit statusChanged(tr("Capture complete"));
        startButton_->setEnabled(true);
        stopButton_->setEnabled(false);
    }
}

void CapturePage::initialize() {
    Page::initialize();
    // Any additional initialization
}

void CapturePage::cleanup() {
    // Any cleanup tasks
    Page::cleanup();
}

} // namespace cam_matrix::ui
