#include "blur_filter.hpp"
#include <QLabel>
#include <QVBoxLayout>
#include <QSlider>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QGraphicsBlurEffect>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsView>
#include <QPainter>

namespace cam_matrix::core {

BlurFilter::BlurFilter()
    : FilterBase(QObject::tr("Blur"), Category::ArtisticFilter)
{
    defaultParameters_["radius"] = 5;
}

QImage BlurFilter::apply(const QImage& image, const QVariantMap& parameters) const {
    // Get the blur radius parameter
    int radius = parameters.value("radius", defaultParameters_["radius"]).toInt();
    
    if (radius <= 0) {
        return image; // No blur needed
    }
    
    // For a proper Gaussian blur, we'll use Qt's QGraphicsBlurEffect
    QGraphicsScene scene;
    QGraphicsPixmapItem item;
    item.setPixmap(QPixmap::fromImage(image));
    scene.addItem(&item);
    
    QGraphicsBlurEffect effect;
    effect.setBlurRadius(radius);
    item.setGraphicsEffect(&effect);
    
    // Render the blurred result
    QImage result(image.size(), QImage::Format_ARGB32);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    scene.render(&painter);
    
    return result;
}

QWidget* BlurFilter::createControlWidget(QWidget* parent) const {
    auto widget = new QWidget(parent);
    auto layout = new QVBoxLayout(widget);
    
    // Radius control
    auto radiusLayout = new QHBoxLayout();
    radiusLayout->addWidget(new QLabel(QObject::tr("Blur Radius:")));
    
    auto radiusSlider = new QSlider(Qt::Horizontal);
    radiusSlider->setRange(0, 20);
    radiusSlider->setValue(defaultParameters_["radius"].toInt());
    radiusSlider->setTickPosition(QSlider::TicksBelow);
    radiusSlider->setTickInterval(5);
    
    auto radiusSpinBox = new QSpinBox();
    radiusSpinBox->setRange(0, 20);
    radiusSpinBox->setValue(defaultParameters_["radius"].toInt());
    
    // Connect the controls
    QObject::connect(radiusSlider, &QSlider::valueChanged, radiusSpinBox, &QSpinBox::setValue);
    QObject::connect(radiusSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), radiusSlider, &QSlider::setValue);
    
    // Store the radius value in the widget for retrieval
    widget->setProperty("getParameters", QVariant::fromValue([radiusSpinBox]() -> QVariantMap {
        QVariantMap params;
        params["radius"] = radiusSpinBox->value();
        return params;
    }));
    
    radiusLayout->addWidget(radiusSlider);
    radiusLayout->addWidget(radiusSpinBox);
    
    layout->addLayout(radiusLayout);
    layout->addStretch();
    
    return widget;
}

} // namespace cam_matrix::core 