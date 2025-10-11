#include "NavigationRail.hpp"

namespace SaperaCapturePro {

NavigationRail::NavigationRail() {}

NavigationItem NavigationRail::Render(NavigationItem current_selection) {
    selected_item_ = current_selection;

    const float button_size = 60.0f;
    const float nav_width = 70.0f;

    // Center buttons horizontally
    const float padding = (nav_width - button_size) * 0.5f;
    ImGui::SetCursorPosX(padding);

    // Spacing from top
    ImGui::Dummy(ImVec2(0, 10));

    // Navigation buttons (using Unicode symbols that render correctly)
    RenderNavButton("◆", "Capture", NavigationItem::Capture);
    ImGui::Dummy(ImVec2(0, 5));

    RenderNavButton("■", "Files", NavigationItem::Files);
    ImGui::Dummy(ImVec2(0, 5));

    RenderNavButton("▣", "Hardware", NavigationItem::Hardware);
    ImGui::Dummy(ImVec2(0, 5));

    RenderNavButton("⚙", "Settings", NavigationItem::Settings);

    return selected_item_;
}

void NavigationRail::RenderNavButton(const char* icon, const char* label, NavigationItem item) {
    const float button_size = 60.0f;
    const float em = ImGui::GetFontSize();
    bool is_selected = (selected_item_ == item);

    // Button styling
    if (is_selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.7f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.6f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.4f, 0.6f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.25f, 0.25f, 0.6f));
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

    // Button with icon and label
    ImGui::BeginGroup();

    if (ImGui::Button(icon, ImVec2(button_size, button_size))) {
        selected_item_ = item;
    }

    // Label below button (centered)
    ImGui::SetCursorPosX((70.0f - ImGui::CalcTextSize(label).x) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, is_selected ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();

    ImGui::EndGroup();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
}

} // namespace SaperaCapturePro
