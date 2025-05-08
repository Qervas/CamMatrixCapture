#include "page.hpp"
#include <QWidget>

namespace cam_matrix::ui {

Page::Page(QWidget* parent)
    : QWidget(parent)
{
}

Page::~Page() = default;

void Page::initialize()
{
    setupUi();
    createConnections();
}

void Page::cleanup()
{
    // Default implementation does nothing
}

} // namespace cam_matrix::ui 