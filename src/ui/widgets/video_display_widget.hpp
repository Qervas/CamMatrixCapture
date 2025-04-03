#pragma once
#include <QWidget>
#include <QImage>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTimer>
#include <mutex>

namespace cam_matrix::ui {

class VideoDisplayWidget : public QWidget {
    Q_OBJECT

public:
    explicit VideoDisplayWidget(QWidget* parent = nullptr);
    ~VideoDisplayWidget() override = default;

public slots:
    void updateFrame(const QImage& frame);
    void clearFrame();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QImage currentFrame_;
    QImage scaledFrame_;
    std::mutex frameMutex_;
    bool frameUpdated_;

    void scaleFrame();
};

} // namespace cam_matrix::ui
