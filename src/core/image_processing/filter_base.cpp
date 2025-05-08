#include "filter_base.hpp"

namespace cam_matrix::core {

FilterBase::FilterBase(const QString& name, Category category)
    : name_(name)
    , category_(category)
{
}

QString FilterBase::name() const {
    return name_;
}

Filter::Category FilterBase::category() const {
    return category_;
}

QVariantMap FilterBase::defaultParameters() const {
    return defaultParameters_;
}

} // namespace cam_matrix::core 