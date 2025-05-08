#include "sepia_filter.hpp"
#include <QLabel>
#include <QVBoxLayout>
#include <QSlider>
#include <QSpinBox>
#include <QHBoxLayout>

namespace cam_matrix::core {

SepiaFilter::SepiaFilter()
    : FilterBase(QObject::tr("Sepia"), Category::ColorEffect)
{
    defaultParameters_["intensity"] = 30; // Range: 0 to 100
}

QImage SepiaFilter::apply(const QImage& image, const QVariantMap& parameters) const {
    // Get intensity parameter (0-100)
    int intensity = parameters.value("intensity", defaultParameters_["intensity"]).toInt();
    
    if (intensity <= 0) {
        return image; // No effect needed
    }
    
    // Create a copy of the image to modify
    QImage result = image.convertToFormat(QImage::Format_ARGB32);
    
    // Calculate intensity factor (0.0-1.0)
    double factor = intensity / 100.0;
    
    // Process each pixel
    for (int y = 0; y < result.height(); ++y) {
        for (int x = 0; x < result.width(); ++x) {
            QRgb pixel = result.pixel(x, y);
            
            // Extract RGB components
            int r = qRed(pixel);
            int g = qGreen(pixel);
            int b = qBlue(pixel);
            int a = qAlpha(pixel);
            
            // Calculate sepia values
            int sepiaR = qBound(0, static_cast<int>((r * (1 - factor * 0.607)) + (g * (factor * 0.769)) + (b * (factor * 0.189))), 255);
            int sepiaG = qBound(0, static_cast<int>((r * (factor * 0.349)) + (g * (1 - factor * 0.314)) + (b * (factor * 0.168))), 255);
            int sepiaB = qBound(0, static_cast<int>((r * (factor * 0.272)) + (g * (factor * 0.534)) + (b * (1 - factor * 0.869))), 255);
            
            // Set the adjusted pixel
            result.setPixel(x, y, qRgba(sepiaR, sepiaG, sepiaB, a));
        }
    }
    
    return result;
}

QWidget* SepiaFilter::createControlWidget(QWidget* parent) const {
    auto widget = new QWidget(parent);
    auto layout = new QVBoxLayout(widget);
    
    // Intensity control
    auto intensityLayout = new QHBoxLayout();
    intensityLayout->addWidget(new QLabel(QObject::tr("Intensity:")));
    
    auto intensitySlider = new QSlider(Qt::Horizontal);
    intensitySlider->setRange(0, 100);
    intensitySlider->setValue(defaultParameters_["intensity"].toInt());
    intensitySlider->setTickPosition(QSlider::TicksBelow);
    intensitySlider->setTickInterval(10);
    
    auto intensitySpinBox = new QSpinBox();
    intensitySpinBox->setRange(0, 100);
    intensitySpinBox->setValue(defaultParameters_["intensity"].toInt());
    intensitySpinBox->setSuffix("%");
    
    // Connect the controls
    QObject::connect(intensitySlider, &QSlider::valueChanged, intensitySpinBox, &QSpinBox::setValue);
    QObject::connect(intensitySpinBox, QOverload<int>::of(&QSpinBox::valueChanged), intensitySlider, &QSlider::setValue);
    
    // Store the intensity value in the widget for retrieval
    widget->setProperty("getParameters", QVariant::fromValue([intensitySpinBox]() -> QVariantMap {
        QVariantMap params;
        params["intensity"] = intensitySpinBox->value();
        return params;
    }));
    
    intensityLayout->addWidget(intensitySlider);
    intensityLayout->addWidget(intensitySpinBox);
    
    layout->addLayout(intensityLayout);
    layout->addStretch();
    
    return widget;
}

} // namespace cam_matrix::core 