#include "sharpen_filter.hpp"
#include <QLabel>
#include <QVBoxLayout>
#include <QSlider>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QPainter>
#include <QImage>

namespace cam_matrix::core {

SharpenFilter::SharpenFilter()
    : FilterBase(QObject::tr("Sharpen"), Category::ArtisticFilter)
{
    defaultParameters_["amount"] = 50;  // Range: 0 to 100
}

QImage SharpenFilter::apply(const QImage& image, const QVariantMap& parameters) const {
    // Get sharpness amount parameter (0-100)
    int amount = parameters.value("amount", defaultParameters_["amount"]).toInt();
    
    if (amount <= 0) {
        return image;
    }
    
    // Convert amount to a factor (0.0 to 2.0)
    double factor = amount / 50.0;
    
    // Create a copy of the image
    QImage result = image.convertToFormat(QImage::Format_ARGB32);
    QImage original = result;
    
    // Apply a simple convolution kernel for sharpening
    // This is a basic unsharp mask implementation
    for (int y = 1; y < result.height() - 1; ++y) {
        for (int x = 1; x < result.width() - 1; ++x) {
            // Get the center pixel and its neighbors
            QRgb center = original.pixel(x, y);
            QRgb top = original.pixel(x, y - 1);
            QRgb bottom = original.pixel(x, y + 1);
            QRgb left = original.pixel(x - 1, y);
            QRgb right = original.pixel(x + 1, y);
            
            // Extract color components of the center
            int centerR = qRed(center);
            int centerG = qGreen(center);
            int centerB = qBlue(center);
            int centerA = qAlpha(center);
            
            // Compute the Laplacian (4-neighborhood)
            int laplacianR = 4 * centerR - qRed(top) - qRed(bottom) - qRed(left) - qRed(right);
            int laplacianG = 4 * centerG - qGreen(top) - qGreen(bottom) - qGreen(left) - qGreen(right);
            int laplacianB = 4 * centerB - qBlue(top) - qBlue(bottom) - qBlue(left) - qBlue(right);
            
            // Apply sharpening by adding a fraction of the Laplacian
            int newR = qBound(0, centerR + static_cast<int>(factor * laplacianR), 255);
            int newG = qBound(0, centerG + static_cast<int>(factor * laplacianG), 255);
            int newB = qBound(0, centerB + static_cast<int>(factor * laplacianB), 255);
            
            // Set the sharpened pixel
            result.setPixel(x, y, qRgba(newR, newG, newB, centerA));
        }
    }
    
    return result;
}

QWidget* SharpenFilter::createControlWidget(QWidget* parent) const {
    auto widget = new QWidget(parent);
    auto layout = new QVBoxLayout(widget);
    
    // Amount control
    auto amountLayout = new QHBoxLayout();
    amountLayout->addWidget(new QLabel(QObject::tr("Sharpness:")));
    
    auto amountSlider = new QSlider(Qt::Horizontal);
    amountSlider->setRange(0, 100);
    amountSlider->setValue(defaultParameters_["amount"].toInt());
    amountSlider->setTickPosition(QSlider::TicksBelow);
    amountSlider->setTickInterval(10);
    
    auto amountSpinBox = new QSpinBox();
    amountSpinBox->setRange(0, 100);
    amountSpinBox->setValue(defaultParameters_["amount"].toInt());
    amountSpinBox->setSuffix("%");
    
    // Connect the controls
    QObject::connect(amountSlider, &QSlider::valueChanged, amountSpinBox, &QSpinBox::setValue);
    QObject::connect(amountSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), amountSlider, &QSlider::setValue);
    
    // Store the amount value in the widget for retrieval
    widget->setProperty("getParameters", QVariant::fromValue([amountSpinBox]() -> QVariantMap {
        QVariantMap params;
        params["amount"] = amountSpinBox->value();
        return params;
    }));
    
    amountLayout->addWidget(amountSlider);
    amountLayout->addWidget(amountSpinBox);
    
    layout->addLayout(amountLayout);
    layout->addStretch();
    
    return widget;
}

} // namespace cam_matrix::core 