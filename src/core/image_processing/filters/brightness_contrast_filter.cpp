#include "brightness_contrast_filter.hpp"
#include <QLabel>
#include <QVBoxLayout>
#include <QSlider>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QPainter>

namespace cam_matrix::core {

BrightnessContrastFilter::BrightnessContrastFilter()
    : FilterBase(QObject::tr("Brightness/Contrast"), Category::BasicAdjustment)
{
    defaultParameters_["brightness"] = 0;  // Range: -100 to 100
    defaultParameters_["contrast"] = 0;    // Range: -100 to 100
}

QImage BrightnessContrastFilter::apply(const QImage& image, const QVariantMap& parameters) const {
    // Get parameters
    int brightness = parameters.value("brightness", defaultParameters_["brightness"]).toInt();
    int contrast = parameters.value("contrast", defaultParameters_["contrast"]).toInt();
    
    // No change needed?
    if (brightness == 0 && contrast == 0) {
        return image;
    }
    
    // Create a copy of the image to modify
    QImage result = image;
    
    // Calculate contrast factor (1.0 means no change)
    double contrastFactor = 1.0 + contrast / 100.0;
    
    // Process each pixel
    for (int y = 0; y < result.height(); ++y) {
        for (int x = 0; x < result.width(); ++x) {
            QRgb pixel = result.pixel(x, y);
            
            // Extract RGB components
            int r = qRed(pixel);
            int g = qGreen(pixel);
            int b = qBlue(pixel);
            
            // Apply brightness
            if (brightness != 0) {
                r += brightness * 255 / 100;
                g += brightness * 255 / 100;
                b += brightness * 255 / 100;
            }
            
            // Apply contrast (adjust relative to middle gray)
            if (contrast != 0) {
                r = 128 + (r - 128) * contrastFactor;
                g = 128 + (g - 128) * contrastFactor;
                b = 128 + (b - 128) * contrastFactor;
            }
            
            // Clamp to valid range
            r = qBound(0, r, 255);
            g = qBound(0, g, 255);
            b = qBound(0, b, 255);
            
            // Set the adjusted pixel
            result.setPixel(x, y, qRgb(r, g, b));
        }
    }
    
    return result;
}

QWidget* BrightnessContrastFilter::createControlWidget(QWidget* parent) const {
    auto widget = new QWidget(parent);
    auto layout = new QVBoxLayout(widget);
    
    // Brightness control
    auto brightnessLayout = new QHBoxLayout();
    brightnessLayout->addWidget(new QLabel(QObject::tr("Brightness:")));
    
    auto brightnessSlider = new QSlider(Qt::Horizontal);
    brightnessSlider->setRange(-100, 100);
    brightnessSlider->setValue(defaultParameters_["brightness"].toInt());
    brightnessSlider->setTickPosition(QSlider::TicksBelow);
    brightnessSlider->setTickInterval(25);
    
    auto brightnessSpinBox = new QSpinBox();
    brightnessSpinBox->setRange(-100, 100);
    brightnessSpinBox->setValue(defaultParameters_["brightness"].toInt());
    brightnessSpinBox->setSuffix("%");
    
    // Connect brightness controls
    QObject::connect(brightnessSlider, &QSlider::valueChanged, brightnessSpinBox, &QSpinBox::setValue);
    QObject::connect(brightnessSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), brightnessSlider, &QSlider::setValue);
    
    brightnessLayout->addWidget(brightnessSlider);
    brightnessLayout->addWidget(brightnessSpinBox);
    
    // Contrast control
    auto contrastLayout = new QHBoxLayout();
    contrastLayout->addWidget(new QLabel(QObject::tr("Contrast:")));
    
    auto contrastSlider = new QSlider(Qt::Horizontal);
    contrastSlider->setRange(-100, 100);
    contrastSlider->setValue(defaultParameters_["contrast"].toInt());
    contrastSlider->setTickPosition(QSlider::TicksBelow);
    contrastSlider->setTickInterval(25);
    
    auto contrastSpinBox = new QSpinBox();
    contrastSpinBox->setRange(-100, 100);
    contrastSpinBox->setValue(defaultParameters_["contrast"].toInt());
    contrastSpinBox->setSuffix("%");
    
    // Connect contrast controls
    QObject::connect(contrastSlider, &QSlider::valueChanged, contrastSpinBox, &QSpinBox::setValue);
    QObject::connect(contrastSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), contrastSlider, &QSlider::setValue);
    
    contrastLayout->addWidget(contrastSlider);
    contrastLayout->addWidget(contrastSpinBox);
    
    // Store parameter values in the widget for retrieval
    widget->setProperty("getParameters", QVariant::fromValue([brightnessSpinBox, contrastSpinBox]() -> QVariantMap {
        QVariantMap params;
        params["brightness"] = brightnessSpinBox->value();
        params["contrast"] = contrastSpinBox->value();
        return params;
    }));
    
    layout->addLayout(brightnessLayout);
    layout->addLayout(contrastLayout);
    layout->addStretch();
    
    return widget;
}

} // namespace cam_matrix::core 