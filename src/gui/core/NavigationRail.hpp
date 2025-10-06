#pragma once

#include <imgui.h>

namespace SaperaCapturePro {

enum class NavigationItem {
    Capture,
    Files,
    Hardware,
    Settings
};

class NavigationRail {
public:
    NavigationRail();
    ~NavigationRail() = default;

    NavigationItem Render(NavigationItem current_selection);
    NavigationItem GetSelectedItem() const { return selected_item_; }
    void SetSelectedItem(NavigationItem item) { selected_item_ = item; }

private:
    NavigationItem selected_item_ = NavigationItem::Capture;

    void RenderNavButton(const char* icon, const char* label, NavigationItem item);
};

} // namespace SaperaCapturePro
