#include "invert_filter.hpp"
#include <QLabel>
#include <QVBoxLayout>

namespace cam_matrix::core {

InvertFilter::InvertFilter()
    : FilterBase(QObject::tr("Invert"), Category::ColorEffect)
{
    // No parameters needed for invert
}

QImage InvertFilter::apply(const QImage& image, const QVariantMap& /*parameters*/) const {
    QImage result = image;
    result.invertPixels();
    return result;
}

QWidget* InvertFilter::createControlWidget(QWidget* parent) const {
    // No parameters to control for invert
    auto widget = new QWidget(parent);
    auto layout = new QVBoxLayout(widget);
    layout->addWidget(new QLabel(QObject::tr("No parameters for invert filter.")));
    return widget;
}

} // namespace cam_matrix::core 