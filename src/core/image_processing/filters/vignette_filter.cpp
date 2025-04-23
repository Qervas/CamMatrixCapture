#include "vignette_filter.hpp"
#include <QLabel>
#include <QVBoxLayout>
#include <QSlider>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QPainter>
#include <QRadialGradient>
#include <QColor>

namespace cam_matrix::core {

VignetteFilter::VignetteFilter()
    : FilterBase(QObject::tr("Vignette"), Category::ArtisticFilter)
{
    defaultParameters_["intensity"] = 50;  // Range: 0 to 100
    defaultParameters_["radius"] = 75;     // Range: 0 to 100 (as percentage of image size)
}

QImage VignetteFilter::apply(const QImage& image, const QVariantMap& parameters) const {
    // Get parameters
    int intensity = parameters.value("intensity", defaultParameters_["intensity"]).toInt();
    int radius = parameters.value("radius", defaultParameters_["radius"]).toInt();
    
    if (intensity <= 0) {
        return image;
    }
    
    // Create a copy of the image
    QImage result = image.convertToFormat(QImage::Format_ARGB32);
    
    // Calculate center and radius
    QPointF center(result.width() / 2.0, result.height() / 2.0);
    double maxDistance = std::sqrt(center.x() * center.x() + center.y() * center.y());
    double vignetteRadius = (radius / 100.0) * maxDistance;
    
    // Calculate intensity as alpha value (0-255)
    int alpha = intensity * 255 / 100;
    
    // Create painter and radial gradient for the vignette
    QPainter painter(&result);
    QRadialGradient gradient(center, maxDistance);
    
    // Set up gradient stops based on radius
    gradient.setColorAt(0, QColor(0, 0, 0, 0));                            // Center: transparent
    gradient.setColorAt(vignetteRadius / maxDistance, QColor(0, 0, 0, 0)); // Start of vignette: transparent
    gradient.setColorAt(1, QColor(0, 0, 0, alpha));                        // Edge: dark with specified intensity
    
    // Apply the vignette
    painter.setCompositionMode(QPainter::CompositionMode_Multiply);
    painter.fillRect(0, 0, result.width(), result.height(), gradient);
    
    return result;
}

QWidget* VignetteFilter::createControlWidget(QWidget* parent) const {
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
    
    // Connect intensity controls
    QObject::connect(intensitySlider, &QSlider::valueChanged, intensitySpinBox, &QSpinBox::setValue);
    QObject::connect(intensitySpinBox, QOverload<int>::of(&QSpinBox::valueChanged), intensitySlider, &QSlider::setValue);
    
    intensityLayout->addWidget(intensitySlider);
    intensityLayout->addWidget(intensitySpinBox);
    
    // Radius control
    auto radiusLayout = new QHBoxLayout();
    radiusLayout->addWidget(new QLabel(QObject::tr("Radius:")));
    
    auto radiusSlider = new QSlider(Qt::Horizontal);
    radiusSlider->setRange(0, 100);
    radiusSlider->setValue(defaultParameters_["radius"].toInt());
    radiusSlider->setTickPosition(QSlider::TicksBelow);
    radiusSlider->setTickInterval(10);
    
    auto radiusSpinBox = new QSpinBox();
    radiusSpinBox->setRange(0, 100);
    radiusSpinBox->setValue(defaultParameters_["radius"].toInt());
    radiusSpinBox->setSuffix("%");
    
    // Connect radius controls
    QObject::connect(radiusSlider, &QSlider::valueChanged, radiusSpinBox, &QSpinBox::setValue);
    QObject::connect(radiusSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), radiusSlider, &QSlider::setValue);
    
    radiusLayout->addWidget(radiusSlider);
    radiusLayout->addWidget(radiusSpinBox);
    
    // Store parameter values in the widget for retrieval
    widget->setProperty("getParameters", QVariant::fromValue([intensitySpinBox, radiusSpinBox]() -> QVariantMap {
        QVariantMap params;
        params["intensity"] = intensitySpinBox->value();
        params["radius"] = radiusSpinBox->value();
        return params;
    }));
    
    layout->addLayout(intensityLayout);
    layout->addLayout(radiusLayout);
    layout->addStretch();
    
    return widget;
}

} // namespace cam_matrix::core 