#pragma once

#include <functional>
#include <string>
#include <map>
#include <vector>

struct ParameterInfo {
    std::string name;
    std::string description;
    std::string unit;
    double minValue;
    double maxValue;
    double currentValue;
    double defaultValue;
    bool isReadOnly;
    bool isSupported;
};

class ParameterPanel {
public:
    ParameterPanel();
    ~ParameterPanel();

    void Initialize();
    void Render();

    // Callbacks
    std::function<void(const std::string&, const std::string&)> onSetParameter;
    std::function<std::vector<ParameterInfo>()> onGetParameters;

    bool visible = true;

private:
    // UI state
    char search_filter_[256] = "";
    bool show_advanced_ = false;
    bool show_read_only_ = false;
    std::map<std::string, double> parameter_values_;
    std::vector<ParameterInfo> cached_parameters_;
    
    // Parameter categories
    enum class ParameterCategory {
        Exposure,
        Gain,
        ROI,
        WhiteBalance,
        Image,
        Trigger,
        Advanced
    };
    
    ParameterCategory current_category_ = ParameterCategory::Exposure;
    
    void RenderParameterCategories();
    void RenderParameterList(const std::vector<ParameterInfo>& parameters);
    void RenderParameterEditor(const ParameterInfo& param);
    void RenderQuickPresets();
    void UpdateParameterCache();
    ParameterCategory GetParameterCategory(const std::string& paramName);
    std::vector<ParameterInfo> FilterParametersByCategory(const std::vector<ParameterInfo>& params, ParameterCategory category);
}; 