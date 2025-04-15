#include "ui/widgets/video_display_widget.hpp"
#include <QPainter>
#include <QApplication>

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
    try {
        // Process the frame update in a safer way - avoid nested mutex locks
        QImage frameCopy = frame.copy();
        
        {
            std::lock_guard<std::mutex> lock(frameMutex_);
            currentFrame_ = frameCopy;
            frameUpdated_ = true;
            
            // Pre-scale the frame while we have the lock - no nested lock call
            if (!currentFrame_.isNull()) {
                // Scale the frame to fit the widget while maintaining aspect ratio
                scaledFrame_ = currentFrame_.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
        }
        
        // Schedule a repaint - don't call update() directly if not on UI thread
        QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
    }
    catch (const std::exception& e) {
        qWarning() << "Exception in VideoDisplayWidget::updateFrame:" << e.what();
    }
    catch (...) {
        qWarning() << "Unknown exception in VideoDisplayWidget::updateFrame";
    }
}

void VideoDisplayWidget::clearFrame() {
    try {
        std::lock_guard<std::mutex> lock(frameMutex_);
        currentFrame_ = QImage();
        scaledFrame_ = QImage();
        frameUpdated_ = false;
        
        // Schedule a repaint - don't call update() directly if not on UI thread
        QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
    }
    catch (const std::exception& e) {
        qWarning() << "Exception in VideoDisplayWidget::clearFrame:" << e.what();
    }
    catch (...) {
        qWarning() << "Unknown exception in VideoDisplayWidget::clearFrame";
    }
}

void VideoDisplayWidget::paintEvent(QPaintEvent* event) {
    try {
        QPainter painter(this);
        QImage frameToDraw;
        
        // Get a copy of the current frame to avoid holding the lock during painting
        {
            std::lock_guard<std::mutex> lock(frameMutex_);
            if (!scaledFrame_.isNull()) {
                frameToDraw = scaledFrame_.copy();
            }
        }

        if (!frameToDraw.isNull()) {
            // Draw the scaled frame centered in the widget
            int x = (width() - frameToDraw.width()) / 2;
            int y = (height() - frameToDraw.height()) / 2;
            painter.drawImage(x, y, frameToDraw);
        } else {
            // Draw "No Signal" text
            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", 20));
            painter.drawText(rect(), Qt::AlignCenter, tr("No Signal"));
        }
    }
    catch (const std::exception& e) {
        qWarning() << "Exception in VideoDisplayWidget::paintEvent:" << e.what();
    }
    catch (...) {
        qWarning() << "Unknown exception in VideoDisplayWidget::paintEvent";
    }
}

void VideoDisplayWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    
    try {
        // Rescale the frame with the new size
        std::lock_guard<std::mutex> lock(frameMutex_);
        if (!currentFrame_.isNull()) {
            // Scale the frame to fit the widget while maintaining aspect ratio
            scaledFrame_ = currentFrame_.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }
    catch (const std::exception& e) {
        qWarning() << "Exception in VideoDisplayWidget::resizeEvent:" << e.what();
    }
    catch (...) {
        qWarning() << "Unknown exception in VideoDisplayWidget::resizeEvent";
    }
}

} // namespace cam_matrix::ui
