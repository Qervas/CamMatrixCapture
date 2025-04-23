#include "saturation_filter.hpp"
#include <QLabel>
#include <QVBoxLayout>
#include <QSlider>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QPainter>
#include <QColor>

namespace cam_matrix::core {

SaturationFilter::SaturationFilter()
    : FilterBase(QObject::tr("Saturation"), Category::BasicAdjustment)
{
    defaultParameters_["saturation"] = 0;  // Range: -100 to 100
}

QImage SaturationFilter::apply(const QImage& image, const QVariantMap& parameters) const {
    // Get saturation parameter (-100 to 100)
    int saturationValue = parameters.value("saturation", defaultParameters_["saturation"]).toInt();
    
    // No change needed?
    if (saturationValue == 0) {
        return image;
    }
    
    // Convert to a factor (0.0 = grayscale, 1.0 = normal, 2.0 = oversaturated)
    double factor = 1.0 + (saturationValue / 100.0);
    
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
            
            // Convert RGB to HSL
            QColor color(r, g, b);
            int h, s, l;
            color.getHsl(&h, &s, &l);
            
            // Adjust saturation
            s = qBound(0, static_cast<int>(s * factor), 255);
            
            // Convert back to RGB
            color.setHsl(h, s, l);
            
            // Set the adjusted pixel
            result.setPixel(x, y, qRgba(color.red(), color.green(), color.blue(), a));
        }
    }
    
    return result;
}

QWidget* SaturationFilter::createControlWidget(QWidget* parent) const {
    auto widget = new QWidget(parent);
    auto layout = new QVBoxLayout(widget);
    
    // Saturation control
    auto saturationLayout = new QHBoxLayout();
    saturationLayout->addWidget(new QLabel(QObject::tr("Saturation:")));
    
    auto saturationSlider = new QSlider(Qt::Horizontal);
    saturationSlider->setRange(-100, 100);
    saturationSlider->setValue(defaultParameters_["saturation"].toInt());
    saturationSlider->setTickPosition(QSlider::TicksBelow);
    saturationSlider->setTickInterval(25);
    
    auto saturationSpinBox = new QSpinBox();
    saturationSpinBox->setRange(-100, 100);
    saturationSpinBox->setValue(defaultParameters_["saturation"].toInt());
    saturationSpinBox->setSuffix("%");
    
    // Connect the controls
    QObject::connect(saturationSlider, &QSlider::valueChanged, saturationSpinBox, &QSpinBox::setValue);
    QObject::connect(saturationSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), saturationSlider, &QSlider::setValue);
    
    // Store the value in the widget for retrieval
    widget->setProperty("getParameters", QVariant::fromValue([saturationSpinBox]() -> QVariantMap {
        QVariantMap params;
        params["saturation"] = saturationSpinBox->value();
        return params;
    }));
    
    saturationLayout->addWidget(saturationSlider);
    saturationLayout->addWidget(saturationSpinBox);
    
    layout->addLayout(saturationLayout);
    layout->addStretch();
    
    return widget;
}

} // namespace cam_matrix::core 