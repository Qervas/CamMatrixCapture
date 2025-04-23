#pragma once

#include "filter.hpp"

namespace cam_matrix::core {

/**
 * @brief Base implementation for filters
 * 
 * This provides default implementations for some of the Filter interface
 * methods to reduce repetition in concrete filter classes.
 */
class FilterBase : public Filter {
public:
    FilterBase(const QString& name, Category category);
    virtual ~FilterBase() = default;

    QString name() const override;
    Category category() const override;
    QVariantMap defaultParameters() const override;

protected:
    QString name_;
    Category category_;
    mutable QVariantMap defaultParameters_;
};

} // namespace cam_matrix::core 