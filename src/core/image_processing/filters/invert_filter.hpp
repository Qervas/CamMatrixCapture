#pragma once

#include "../filter_base.hpp"

namespace cam_matrix::core {

/**
 * @brief Invert filter
 * 
 * Inverts the colors of an image.
 */
class InvertFilter : public FilterBase {
public:
    InvertFilter();
    
    QImage apply(const QImage& image, const QVariantMap& parameters) const override;
    QWidget* createControlWidget(QWidget* parent) const override;
};

} // namespace cam_matrix::core 