#pragma once

#include "../filter_base.hpp"

namespace cam_matrix::core {

/**
 * @brief Sepia filter
 * 
 * Applies a sepia tone effect to an image.
 */
class SepiaFilter : public FilterBase {
public:
    SepiaFilter();
    
    QImage apply(const QImage& image, const QVariantMap& parameters) const override;
    QWidget* createControlWidget(QWidget* parent) const override;
};

} // namespace cam_matrix::core 