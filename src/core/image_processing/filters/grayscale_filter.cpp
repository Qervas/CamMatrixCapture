#include "grayscale_filter.hpp"
#include <QLabel>
#include <QVBoxLayout>

namespace cam_matrix::core {

GrayscaleFilter::GrayscaleFilter()
    : FilterBase(QObject::tr("Grayscale"), Category::ColorEffect)
{
    // No parameters needed for grayscale conversion
}

QImage GrayscaleFilter::apply(const QImage& image, const QVariantMap& /*parameters*/) const {
    // Simple grayscale conversion - we'll use Qt's built-in function
    return image.convertToFormat(QImage::Format_Grayscale8);
}

QWidget* GrayscaleFilter::createControlWidget(QWidget* parent) const {
    // No parameters to control for grayscale
    auto widget = new QWidget(parent);
    auto layout = new QVBoxLayout(widget);
    layout->addWidget(new QLabel(QObject::tr("No parameters for grayscale conversion.")));
    return widget;
}

} // namespace cam_matrix::core 