#pragma once

#include "../filter_base.hpp"

namespace cam_matrix::core {

/**
 * @brief Sharpen filter
 * 
 * Enhances the details in an image by increasing edge contrast.
 */
class SharpenFilter : public FilterBase {
public:
    SharpenFilter();
    
    QImage apply(const QImage& image, const QVariantMap& parameters) const override;
    QWidget* createControlWidget(QWidget* parent) const override;
};

} // namespace cam_matrix::core 