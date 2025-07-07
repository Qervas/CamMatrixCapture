#include "ParameterPanel.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>

ParameterPanel::ParameterPanel() {
    std::memset(search_filter_, 0, sizeof(search_filter_));
}

ParameterPanel::~ParameterPanel() = default;

void ParameterPanel::Initialize() {
    UpdateParameterCache();
}

void ParameterPanel::Render() {
    if (!visible) return;

    ImGui::Begin("Camera Parameters", &visible);

    // Update parameter cache periodically
    static int frame_counter = 0;
    if (++frame_counter % 60 == 0) { // Update every 60 frames (~1 second at 60fps)
        UpdateParameterCache();
    }

    // Search filter
    ImGui::Text("Search:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputText("##ParameterSearch", search_filter_, sizeof(search_filter_));
    
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        std::memset(search_filter_, 0, sizeof(search_filter_));
    }

    // Show options
    ImGui::SameLine();
    ImGui::Checkbox("Advanced", &show_advanced_);
    ImGui::SameLine();
    ImGui::Checkbox("Read-only", &show_read_only_);

    ImGui::Separator();

    // Quick presets
    RenderQuickPresets();
    
    ImGui::Separator();

    // Parameter categories and list
    ImGui::BeginChild("ParameterContent", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    
    // Category tabs
    RenderParameterCategories();
    
    ImGui::Separator();
    
    // Filter and display parameters
    auto filtered_params = FilterParametersByCategory(cached_parameters_, current_category_);
    RenderParameterList(filtered_params);
    
    ImGui::EndChild();

    ImGui::End();
}

void ParameterPanel::RenderQuickPresets() {
    ImGui::Text("Quick Presets:");
    
    if (ImGui::Button("Indoor Lighting")) {
        if (onSetParameter) {
            onSetParameter("ExposureTime", "50000");  // 50ms
            onSetParameter("Gain", "2.0");
            onSetParameter("Gamma", "1.0");
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Outdoor Bright")) {
        if (onSetParameter) {
            onSetParameter("ExposureTime", "10000");  // 10ms
            onSetParameter("Gain", "1.0");
            onSetParameter("Gamma", "0.8");
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("High Speed")) {
        if (onSetParameter) {
            onSetParameter("ExposureTime", "1000");   // 1ms
            onSetParameter("Gain", "4.0");
            onSetParameter("Gamma", "1.2");
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Reset to Defaults")) {
        for (const auto& param : cached_parameters_) {
            if (!param.isReadOnly) {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(3) << param.defaultValue;
                if (onSetParameter) {
                    onSetParameter(param.name, oss.str());
                }
            }
        }
    }
}

void ParameterPanel::RenderParameterCategories() {
    const char* category_names[] = {
        "Exposure", "Gain", "ROI", "White Balance", "Image", "Trigger", "Advanced"
    };
    
    for (int i = 0; i < 7; ++i) {
        if (i > 0) ImGui::SameLine();
        
        bool is_selected = (current_category_ == static_cast<ParameterCategory>(i));
        if (is_selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.33f, 0.67f, 0.86f, 1.0f));
        }
        
        if (ImGui::Button(category_names[i], ImVec2(100, 0))) {
            current_category_ = static_cast<ParameterCategory>(i);
        }
        
        if (is_selected) {
            ImGui::PopStyleColor();
        }
    }
}

void ParameterPanel::RenderParameterList(const std::vector<ParameterInfo>& parameters) {
    // Filter parameters based on search text
    std::vector<ParameterInfo> filtered_params;
    std::string filter_lower = search_filter_;
    std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(), ::tolower);
    
    for (const auto& param : parameters) {
        // Skip read-only parameters if not showing them
        if (param.isReadOnly && !show_read_only_) continue;
        
        // Skip advanced parameters if not showing them
        if (!show_advanced_ && GetParameterCategory(param.name) == ParameterCategory::Advanced) continue;
        
        // Apply search filter
        if (!filter_lower.empty()) {
            std::string name_lower = param.name;
            std::string desc_lower = param.description;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
            std::transform(desc_lower.begin(), desc_lower.end(), desc_lower.begin(), ::tolower);
            
            if (name_lower.find(filter_lower) == std::string::npos &&
                desc_lower.find(filter_lower) == std::string::npos) {
                continue;
            }
        }
        
        filtered_params.push_back(param);
    }

    // Render parameter table
    if (ImGui::BeginTable("ParametersTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | 
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
        
        ImGui::TableSetupColumn("Parameter", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Range", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const auto& param : filtered_params) {
            ImGui::TableNextRow();
            
            // Parameter name
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", param.name.c_str());
            if (param.isReadOnly) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "(RO)");
            }
            
            // Value editor
            ImGui::TableSetColumnIndex(1);
            RenderParameterEditor(param);
            
            // Range
            ImGui::TableSetColumnIndex(2);
            if (param.minValue != param.maxValue) {
                ImGui::Text("%.3f - %.3f %s", param.minValue, param.maxValue, param.unit.c_str());
            } else {
                ImGui::Text("%s", param.unit.c_str());
            }
            
            // Description
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%s", param.description.c_str());
        }
        
        ImGui::EndTable();
    }
}

void ParameterPanel::RenderParameterEditor(const ParameterInfo& param) {
    ImGui::PushID(param.name.c_str());
    
    if (param.isReadOnly) {
        // Read-only value display
        ImGui::Text("%.3f", param.currentValue);
    } else {
        // Editable value
        double value = param.currentValue;
        
        if (param.minValue != param.maxValue) {
            // Slider for parameters with range
            if (ImGui::SliderScalar("##value", ImGuiDataType_Double, &value, 
                                   &param.minValue, &param.maxValue, "%.3f")) {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(3) << value;
                if (onSetParameter) {
                    onSetParameter(param.name, oss.str());
                }
            }
        } else {
            // Input field for parameters without range
            if (ImGui::InputDouble("##value", &value, 0.0, 0.0, "%.3f")) {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(3) << value;
                if (onSetParameter) {
                    onSetParameter(param.name, oss.str());
                }
            }
        }
        
        // Reset to default button
        ImGui::SameLine();
        if (ImGui::Button("↺", ImVec2(20, 0))) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3) << param.defaultValue;
            if (onSetParameter) {
                onSetParameter(param.name, oss.str());
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reset to default: %.3f", param.defaultValue);
        }
    }
    
    ImGui::PopID();
}

void ParameterPanel::UpdateParameterCache() {
    if (onGetParameters) {
        cached_parameters_ = onGetParameters();
    }
}

ParameterPanel::ParameterCategory ParameterPanel::GetParameterCategory(const std::string& paramName) {
    // Categorize parameters based on their names
    if (paramName.find("Exposure") != std::string::npos || 
        paramName.find("ExposureTime") != std::string::npos) {
        return ParameterCategory::Exposure;
    }
    if (paramName.find("Gain") != std::string::npos) {
        return ParameterCategory::Gain;
    }
    if (paramName.find("Width") != std::string::npos || 
        paramName.find("Height") != std::string::npos ||
        paramName.find("OffsetX") != std::string::npos ||
        paramName.find("OffsetY") != std::string::npos ||
        paramName.find("ROI") != std::string::npos) {
        return ParameterCategory::ROI;
    }
    if (paramName.find("WhiteBalance") != std::string::npos ||
        paramName.find("BalanceRatio") != std::string::npos) {
        return ParameterCategory::WhiteBalance;
    }
    if (paramName.find("Gamma") != std::string::npos ||
        paramName.find("Brightness") != std::string::npos ||
        paramName.find("Contrast") != std::string::npos ||
        paramName.find("Saturation") != std::string::npos ||
        paramName.find("Hue") != std::string::npos) {
        return ParameterCategory::Image;
    }
    if (paramName.find("Trigger") != std::string::npos) {
        return ParameterCategory::Trigger;
    }
    
    return ParameterCategory::Advanced;
}

std::vector<ParameterInfo> ParameterPanel::FilterParametersByCategory(
    const std::vector<ParameterInfo>& params, ParameterCategory category) {
    
    std::vector<ParameterInfo> filtered;
    
    for (const auto& param : params) {
        if (GetParameterCategory(param.name) == category) {
            filtered.push_back(param);
        }
    }
    
    return filtered;
} 