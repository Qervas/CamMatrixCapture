#include "ui/widgets/video_display_widget.hpp"
#include <QPainter>

namespace cam_matrix::ui {

VideoDisplayWidget::VideoDisplayWidget(QWidget* parent)
    : QWidget(parent)
    , frameUpdated_(false)
{
    // Set black background
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);

    // Enable mouse tracking for future interaction
    setMouseTracking(true);

    // Set minimum size
    setMinimumSize(320, 240);
}

void VideoDisplayWidget::updateFrame(const QImage& frame) {
    std::lock_guard<std::mutex> lock(frameMutex_);
    currentFrame_ = frame.copy();
    frameUpdated_ = true;
    scaleFrame();
    update(); // Schedule a repaint
}

void VideoDisplayWidget::clearFrame() {
    std::lock_guard<std::mutex> lock(frameMutex_);
    currentFrame_ = QImage();
    scaledFrame_ = QImage();
    frameUpdated_ = false;
    update();
}

void VideoDisplayWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);

    if (!scaledFrame_.isNull()) {
        // Draw the scaled frame centered in the widget
        int x = (width() - scaledFrame_.width()) / 2;
        int y = (height() - scaledFrame_.height()) / 2;
        painter.drawImage(x, y, scaledFrame_);
    } else {
        // Draw "No Signal" text
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 20));
        painter.drawText(rect(), Qt::AlignCenter, tr("No Signal"));
    }
}

void VideoDisplayWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    scaleFrame();
}

void VideoDisplayWidget::scaleFrame() {
    std::lock_guard<std::mutex> lock(frameMutex_);
    if (!currentFrame_.isNull()) {
        // Scale the frame to fit the widget while maintaining aspect ratio
        scaledFrame_ = currentFrame_.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
}

} // namespace cam_matrix::ui
