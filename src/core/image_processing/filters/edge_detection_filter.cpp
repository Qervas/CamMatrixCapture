#include "edge_detection_filter.hpp"
#include <QLabel>
#include <QVBoxLayout>
#include <QSlider>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QPainter>
#include <QImage>
#include <cmath>

namespace cam_matrix::core {

EdgeDetectionFilter::EdgeDetectionFilter()
    : FilterBase(QObject::tr("Edge Detection"), Category::ArtisticFilter)
{
    defaultParameters_["threshold"] = 30;     // Range: 0 to 100
    defaultParameters_["invert"] = false;     // Whether to invert the result
}

QImage EdgeDetectionFilter::apply(const QImage& image, const QVariantMap& parameters) const {
    // Get parameters
    int threshold = parameters.value("threshold", defaultParameters_["threshold"]).toInt();
    bool invert = parameters.value("invert", defaultParameters_["invert"]).toBool();
    
    // First convert to grayscale for simplicity
    QImage grayscale = image.convertToFormat(QImage::Format_Grayscale8);
    
    // Create a copy for our result
    QImage result(image.width(), image.height(), QImage::Format_ARGB32);
    
    // Apply Sobel operator (3x3 convolution kernels for X and Y directions)
    for (int y = 1; y < grayscale.height() - 1; ++y) {
        for (int x = 1; x < grayscale.width() - 1; ++x) {
            // Get the 3x3 neighborhood
            int p00 = qGray(grayscale.pixel(x-1, y-1));
            int p10 = qGray(grayscale.pixel(x, y-1));
            int p20 = qGray(grayscale.pixel(x+1, y-1));
            int p01 = qGray(grayscale.pixel(x-1, y));
            int p21 = qGray(grayscale.pixel(x+1, y));
            int p02 = qGray(grayscale.pixel(x-1, y+1));
            int p12 = qGray(grayscale.pixel(x, y+1));
            int p22 = qGray(grayscale.pixel(x+1, y+1));
            
            // Sobel X kernel:
            // [-1  0  1]
            // [-2  0  2]
            // [-1  0  1]
            int sobelX = -p00 - 2*p01 - p02 + p20 + 2*p21 + p22;
            
            // Sobel Y kernel:
            // [-1 -2 -1]
            // [ 0  0  0]
            // [ 1  2  1]
            int sobelY = -p00 - 2*p10 - p20 + p02 + 2*p12 + p22;
            
            // Calculate gradient magnitude
            double gradientMagnitude = std::sqrt(sobelX * sobelX + sobelY * sobelY);
            
            // Normalize to 0-255 range (empirical factor based on Sobel)
            int normalizedGradient = qBound(0, static_cast<int>(gradientMagnitude * 255.0 / 1442.0), 255);
            
            // Apply threshold
            int thresholdValue = threshold * 255 / 100;
            int edgeValue = normalizedGradient < thresholdValue ? 0 : 255;
            
            // Handle inversion if requested
            if (invert) {
                edgeValue = 255 - edgeValue;
            }
            
            // Set the result pixel
            result.setPixel(x, y, qRgba(edgeValue, edgeValue, edgeValue, qAlpha(image.pixel(x, y))));
        }
    }
    
    // Handle border pixels (set to black or white based on invert)
    int borderValue = invert ? 255 : 0;
    for (int x = 0; x < result.width(); ++x) {
        result.setPixel(x, 0, qRgba(borderValue, borderValue, borderValue, qAlpha(image.pixel(x, 0))));
        result.setPixel(x, result.height()-1, qRgba(borderValue, borderValue, borderValue, qAlpha(image.pixel(x, result.height()-1))));
    }
    for (int y = 0; y < result.height(); ++y) {
        result.setPixel(0, y, qRgba(borderValue, borderValue, borderValue, qAlpha(image.pixel(0, y))));
        result.setPixel(result.width()-1, y, qRgba(borderValue, borderValue, borderValue, qAlpha(image.pixel(result.width()-1, y))));
    }
    
    return result;
}

QWidget* EdgeDetectionFilter::createControlWidget(QWidget* parent) const {
    auto widget = new QWidget(parent);
    auto layout = new QVBoxLayout(widget);
    
    // Threshold control
    auto thresholdLayout = new QHBoxLayout();
    thresholdLayout->addWidget(new QLabel(QObject::tr("Threshold:")));
    
    auto thresholdSlider = new QSlider(Qt::Horizontal);
    thresholdSlider->setRange(0, 100);
    thresholdSlider->setValue(defaultParameters_["threshold"].toInt());
    thresholdSlider->setTickPosition(QSlider::TicksBelow);
    thresholdSlider->setTickInterval(10);
    
    auto thresholdSpinBox = new QSpinBox();
    thresholdSpinBox->setRange(0, 100);
    thresholdSpinBox->setValue(defaultParameters_["threshold"].toInt());
    thresholdSpinBox->setSuffix("%");
    
    // Connect threshold controls
    QObject::connect(thresholdSlider, &QSlider::valueChanged, thresholdSpinBox, &QSpinBox::setValue);
    QObject::connect(thresholdSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), thresholdSlider, &QSlider::setValue);
    
    thresholdLayout->addWidget(thresholdSlider);
    thresholdLayout->addWidget(thresholdSpinBox);
    
    // Invert checkbox
    auto invertCheckBox = new QCheckBox(QObject::tr("Invert Result"));
    invertCheckBox->setChecked(defaultParameters_["invert"].toBool());
    
    // Store parameter values in the widget for retrieval
    widget->setProperty("getParameters", QVariant::fromValue([thresholdSpinBox, invertCheckBox]() -> QVariantMap {
        QVariantMap params;
        params["threshold"] = thresholdSpinBox->value();
        params["invert"] = invertCheckBox->isChecked();
        return params;
    }));
    
    layout->addLayout(thresholdLayout);
    layout->addWidget(invertCheckBox);
    layout->addStretch();
    
    return widget;
}

} // namespace cam_matrix::core 