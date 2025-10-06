#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <imgui.h>
#include <glad/glad.h>
#include "../../hardware/CameraManager.hpp"
#include "../../utils/SettingsManager.hpp"

namespace SaperaCapturePro {

struct CameraPreview {
    std::string camera_id;
    std::string camera_name;
    GLuint texture_id = 0;
    int image_width = 0;
    int image_height = 0;
    bool has_preview = false;
    std::string preview_path;

    // Crop rectangle (in image coordinates)
    int crop_x = 0;
    int crop_y = 0;
    int crop_w = 4112;
    int crop_h = 3008;

    // UI state for crop interaction
    bool is_dragging_crop = false;
    enum DragMode { None, Move, ResizeTopLeft, ResizeTopRight, ResizeBottomLeft, ResizeBottomRight, ResizeTop, ResizeBottom, ResizeLeft, ResizeRight };
    DragMode drag_mode = None;
    ImVec2 drag_start_pos;
    int drag_start_crop_x, drag_start_crop_y, drag_start_crop_w, drag_start_crop_h;
};

class CameraPreviewWidget {
public:
    CameraPreviewWidget();
    ~CameraPreviewWidget();

    void Initialize(CameraManager* camera_mgr, SettingsManager* settings_mgr);
    void RenderContent();  // Render without window wrapper
    void Shutdown();

    // Actions
    void CapturePreviewImages();
    void ResetAllCrops();
    void ApplyCropToAllCameras();  // Apply current camera's crop to all
    void ApplyCropToCamera(const std::string& camera_id);
    void LoadCropSettings();
    void SaveCropSettings();

    // Callbacks
    void SetLogCallback(std::function<void(const std::string&)> callback) {
        log_callback_ = callback;
    }

private:
    // System references
    CameraManager* camera_manager_ = nullptr;
    SettingsManager* settings_manager_ = nullptr;

    // State
    std::vector<CameraPreview> camera_previews_;
    bool is_capturing_ = false;
    bool maintain_aspect_ratio_ = true;
    float global_aspect_ratio_ = 1.37f; // 4112/3008
    int current_camera_index_ = 0;  // For single camera view navigation

#ifdef _WIN32
    // GDI+ state for TIFF loading
    ULONG_PTR gdiplus_token_ = 0;
    bool gdiplus_initialized_ = false;
#endif

    // Callbacks
    std::function<void(const std::string&)> log_callback_;

    // Rendering methods
    void RenderToolbar();
    void RenderCameraGrid();
    void RenderCameraPreview(CameraPreview& preview, const ImVec2& cell_size);
    void RenderCropOverlay(CameraPreview& preview, const ImVec2& display_pos, const ImVec2& display_size, float scale);

    // Helper methods
    void UpdateCameraPreviews();
    void ClearPreviews();
    bool LoadPreviewImage(CameraPreview& preview, const std::string& image_path);
    void UnloadPreviewTexture(CameraPreview& preview);
    int GetGridColumns() const;
    int GetGridRows() const;

    // Crop interaction
    void HandleCropInteraction(CameraPreview& preview, const ImVec2& display_pos, const ImVec2& display_size, float scale);
    CameraPreview::DragMode GetDragMode(const CameraPreview& preview, const ImVec2& mouse_pos, const ImVec2& crop_min, const ImVec2& crop_max, float handle_size);
    void UpdateCropFromDrag(CameraPreview& preview, const ImVec2& mouse_delta, float scale);
    void ClampCropRect(CameraPreview& preview);

    // Utility
    void LogMessage(const std::string& message);
};

} // namespace SaperaCapturePro
