#include "threshold_filter.hpp"
#include <QLabel>
#include <QVBoxLayout>
#include <QSlider>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QPainter>
#include <QImage>

namespace cam_matrix::core {

ThresholdFilter::ThresholdFilter()
    : FilterBase(QObject::tr("Threshold"), Category::ColorEffect)
{
    defaultParameters_["threshold"] = 128;  // Range: 0 to 255
}

QImage ThresholdFilter::apply(const QImage& image, const QVariantMap& parameters) const {
    // Get threshold parameter (0-255)
    int threshold = parameters.value("threshold", defaultParameters_["threshold"]).toInt();
    
    // Create a copy of the image
    QImage result = image.convertToFormat(QImage::Format_ARGB32);
    
    // Process each pixel
    for (int y = 0; y < result.height(); ++y) {
        for (int x = 0; x < result.width(); ++x) {
            QRgb pixel = result.pixel(x, y);
            
            // Extract color components
            int r = qRed(pixel);
            int g = qGreen(pixel);
            int b = qBlue(pixel);
            int a = qAlpha(pixel);
            
            // Calculate luminance (grayscale value)
            int luminance = (r * 299 + g * 587 + b * 114) / 1000;
            
            // Apply threshold
            int value = (luminance >= threshold) ? 255 : 0;
            
            // Set the thresholded pixel
            result.setPixel(x, y, qRgba(value, value, value, a));
        }
    }
    
    return result;
}

QWidget* ThresholdFilter::createControlWidget(QWidget* parent) const {
    auto widget = new QWidget(parent);
    auto layout = new QVBoxLayout(widget);
    
    // Threshold control
    auto thresholdLayout = new QHBoxLayout();
    thresholdLayout->addWidget(new QLabel(QObject::tr("Threshold:")));
    
    auto thresholdSlider = new QSlider(Qt::Horizontal);
    thresholdSlider->setRange(0, 255);
    thresholdSlider->setValue(defaultParameters_["threshold"].toInt());
    thresholdSlider->setTickPosition(QSlider::TicksBelow);
    thresholdSlider->setTickInterval(32);
    
    auto thresholdSpinBox = new QSpinBox();
    thresholdSpinBox->setRange(0, 255);
    thresholdSpinBox->setValue(defaultParameters_["threshold"].toInt());
    
    // Connect the controls
    QObject::connect(thresholdSlider, &QSlider::valueChanged, thresholdSpinBox, &QSpinBox::setValue);
    QObject::connect(thresholdSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), thresholdSlider, &QSlider::setValue);
    
    // Store the threshold value in the widget for retrieval
    widget->setProperty("getParameters", QVariant::fromValue([thresholdSpinBox]() -> QVariantMap {
        QVariantMap params;
        params["threshold"] = thresholdSpinBox->value();
        return params;
    }));
    
    thresholdLayout->addWidget(thresholdSlider);
    thresholdLayout->addWidget(thresholdSpinBox);
    
    layout->addLayout(thresholdLayout);
    layout->addStretch();
    
    return widget;
}

} // namespace cam_matrix::core 