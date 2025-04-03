#include "mock_camera.hpp"
#include <QDateTime>
#include <QPainter>
#include <QFont>

namespace cam_matrix::core {

MockCamera::MockCamera(int id, const std::string& name)
    : id_(id)
    , name_(name)
    , isConnected_(false)
    , rng_(std::random_device{}())
{
    // Generate some random colors for our test pattern
    for (int i = 0; i < 10; ++i) {
        colors_.push_back(QColor::fromRgb(
            rng_() % 256, rng_() % 256, rng_() % 256));
    }

    // Create initial frame
    currentFrame_ = QImage(640, 480, QImage::Format_RGB32);
    currentFrame_.fill(Qt::black);

    // Setup timer for frame generation
    QObject::connect(&frameTimer_, &QTimer::timeout, this, &MockCamera::generateFrame);
}

MockCamera::~MockCamera() {
    if (isConnected_) {
        disconnectCamera();
    }
}

bool MockCamera::connectCamera() {
    if (!isConnected_) {
        isConnected_ = true;
        frameTimer_.start(33); // ~30 fps
        return true;
    }
    return false;
}

bool MockCamera::disconnectCamera() {
    if (isConnected_) {
        frameTimer_.stop();
        isConnected_ = false;
        return true;
    }
    return false;
}

QImage MockCamera::getFrame() {
    return currentFrame_;
}

void MockCamera::generateFrame() {
    createTestPattern();
    emit newFrameAvailable(currentFrame_);
}

void MockCamera::createTestPattern() {
    // Create a simple test pattern
    currentFrame_.fill(Qt::black);

    QPainter painter(&currentFrame_);

    // Draw moving circle
    static int pos = 0;
    pos = (pos + 5) % currentFrame_.width();

    painter.setPen(Qt::NoPen);
    painter.setBrush(colors_[id_ % colors_.size()]);
    painter.drawEllipse(pos, currentFrame_.height() / 2, 50, 50);

    // Draw camera info
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 20));
    painter.drawText(20, 40, QString::fromStdString(name_));

    // Draw timestamp
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    painter.drawText(20, currentFrame_.height() - 20, timestamp);

    // Draw moving pattern based on camera ID
    for (int i = 0; i < 5; i++) {
        int y = (currentFrame_.height() / 6) * (i + 1);
        int x = (pos + id_ * 30) % currentFrame_.width();
        painter.setBrush(colors_[(id_ + i) % colors_.size()]);
        painter.drawRect(x, y - 15, 30, 30);
    }
}

} // namespace cam_matrix::core
