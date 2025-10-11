#include "CameraPreviewWidget.hpp"
#include "../LogPanel.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;
#endif

namespace SaperaCapturePro {

CameraPreviewWidget::CameraPreviewWidget() {
#ifdef _WIN32
    // Initialize GDI+ for TIFF loading
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplus_token_, &gdiplusStartupInput, nullptr);
    gdiplus_initialized_ = true;
#endif
}

CameraPreviewWidget::~CameraPreviewWidget() {
    Shutdown();
}

void CameraPreviewWidget::Initialize(CameraManager* camera_mgr, SettingsManager* settings_mgr) {
    camera_manager_ = camera_mgr;
    settings_manager_ = settings_mgr;
    UpdateCameraPreviews();
    LoadCropSettings();
}

void CameraPreviewWidget::Shutdown() {
    ClearPreviews();

#ifdef _WIN32
    if (gdiplus_initialized_) {
        GdiplusShutdown(gdiplus_token_);
        gdiplus_initialized_ = false;
    }
#endif
}

void CameraPreviewWidget::RenderContent() {
    if (!camera_manager_ || !settings_manager_) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "❌ Camera Preview not initialized");
        return;
    }

    // Update camera list if empty or camera count changed
    auto discovered = camera_manager_->GetDiscoveredCameras();
    if (camera_previews_.size() != discovered.size()) {
        UpdateCameraPreviews();
        LoadCropSettings();
    }

    RenderToolbar();
    ImGui::Separator();
    RenderCameraGrid();
}

void CameraPreviewWidget::RenderToolbar() {
    // Capture preview button
    if (ImGui::Button("📷 Capture Preview Images", ImVec2(200, 0))) {
        CapturePreviewImages();
    }
    ImGui::SameLine();

    // Reset crops button
    if (ImGui::Button("↺ Reset All Crops", ImVec2(150, 0))) {
        ResetAllCrops();
    }
    ImGui::SameLine();

    // Apply crop to all cameras button
    if (ImGui::Button("📋 Apply to All Cameras", ImVec2(180, 0))) {
        ApplyCropToAllCameras();
    }
    ImGui::SameLine();

    // Maintain aspect ratio toggle
    ImGui::Checkbox("Maintain Aspect Ratio", &maintain_aspect_ratio_);

    // Status text
    if (is_capturing_) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "⏳ Capturing...");
    }
}

void CameraPreviewWidget::RenderCameraGrid() {
    if (camera_previews_.empty()) {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "No cameras available. Click 'Capture Preview Images' to generate previews.");
        return;
    }

    // Clamp current index
    if (current_camera_index_ < 0) current_camera_index_ = 0;
    if (current_camera_index_ >= camera_previews_.size()) current_camera_index_ = camera_previews_.size() - 1;

    // Navigation arrows and camera selector
    const float em = ImGui::GetFontSize();
    float button_width = 3.0f * em;

    // Previous button
    if (ImGui::Button("◀", ImVec2(button_width, 0))) {
        current_camera_index_--;
        if (current_camera_index_ < 0) current_camera_index_ = camera_previews_.size() - 1;
    }

    ImGui::SameLine();

    // Camera dropdown selector
    ImGui::SetNextItemWidth(15.0f * em);
    if (ImGui::BeginCombo("##camera_select", camera_previews_[current_camera_index_].camera_name.c_str())) {
        for (int i = 0; i < camera_previews_.size(); i++) {
            bool is_selected = (i == current_camera_index_);
            if (ImGui::Selectable(camera_previews_[i].camera_name.c_str(), is_selected)) {
                current_camera_index_ = i;
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();

    // Next button
    if (ImGui::Button("▶", ImVec2(button_width, 0))) {
        current_camera_index_++;
        if (current_camera_index_ >= camera_previews_.size()) current_camera_index_ = 0;
    }

    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(%d / %d)", current_camera_index_ + 1, (int)camera_previews_.size());

    ImGui::Separator();

    // Render the current camera in full size
    ImVec2 available_size = ImGui::GetContentRegionAvail();
    RenderCameraPreview(camera_previews_[current_camera_index_], available_size);
}

void CameraPreviewWidget::RenderCameraPreview(CameraPreview& preview, const ImVec2& cell_size) {
    // Calculate display area
    ImVec2 available = cell_size;
    float aspect_ratio = preview.image_width > 0 ? (float)preview.image_width / preview.image_height : 1.37f;

    ImVec2 display_size;
    if (available.x / available.y > aspect_ratio) {
        display_size.y = available.y;
        display_size.x = available.y * aspect_ratio;
    } else {
        display_size.x = available.x;
        display_size.y = available.x / aspect_ratio;
    }

    // Center the image
    float offset_x = (available.x - display_size.x) * 0.5f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);
    ImVec2 display_pos = ImGui::GetCursorScreenPos();

    // Render preview image or placeholder
    if (preview.has_preview && preview.texture_id != 0) {
        // Use invisible button to capture mouse input over the image
        ImGui::SetCursorScreenPos(display_pos);
        ImGui::InvisibleButton("##image_interaction", display_size);

        // Draw image using DrawList so it doesn't interfere with button
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddImage((void*)(intptr_t)preview.texture_id, display_pos,
                          ImVec2(display_pos.x + display_size.x, display_pos.y + display_size.y));

        // Calculate scale for crop coordinates
        float scale = (float)preview.image_width / display_size.x;

        // Render crop overlay
        RenderCropOverlay(preview, display_pos, display_size, scale);
    } else {
        // Placeholder - use InvisibleButton to reserve space
        ImGui::InvisibleButton("##placeholder", display_size);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(display_pos, ImVec2(display_pos.x + display_size.x, display_pos.y + display_size.y), IM_COL32(40, 40, 40, 255));

        ImVec2 text_pos(display_pos.x + display_size.x * 0.5f - 50, display_pos.y + display_size.y * 0.5f - 10);
        draw_list->AddText(text_pos, IM_COL32(128, 128, 128, 255), "No Preview");
    }
}

void CameraPreviewWidget::RenderCropOverlay(CameraPreview& preview, const ImVec2& display_pos, const ImVec2& display_size, float scale) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Convert crop rect from image coordinates to screen coordinates
    ImVec2 crop_min(
        display_pos.x + preview.crop_x / scale,
        display_pos.y + preview.crop_y / scale
    );
    ImVec2 crop_max(
        display_pos.x + (preview.crop_x + preview.crop_w) / scale,
        display_pos.y + (preview.crop_y + preview.crop_h) / scale
    );

    // Draw semi-transparent overlay outside crop area
    ImU32 overlay_color = IM_COL32(0, 0, 0, 128);

    // Top rectangle
    if (crop_min.y > display_pos.y) {
        draw_list->AddRectFilled(display_pos, ImVec2(display_pos.x + display_size.x, crop_min.y), overlay_color);
    }
    // Bottom rectangle
    if (crop_max.y < display_pos.y + display_size.y) {
        draw_list->AddRectFilled(ImVec2(display_pos.x, crop_max.y), ImVec2(display_pos.x + display_size.x, display_pos.y + display_size.y), overlay_color);
    }
    // Left rectangle
    if (crop_min.x > display_pos.x) {
        draw_list->AddRectFilled(ImVec2(display_pos.x, crop_min.y), ImVec2(crop_min.x, crop_max.y), overlay_color);
    }
    // Right rectangle
    if (crop_max.x < display_pos.x + display_size.x) {
        draw_list->AddRectFilled(ImVec2(crop_max.x, crop_min.y), ImVec2(display_pos.x + display_size.x, crop_max.y), overlay_color);
    }

    // Draw crop rectangle border
    draw_list->AddRect(crop_min, crop_max, IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.0f);

    // Draw corner handles
    float handle_size = 8.0f;
    ImU32 handle_color = IM_COL32(255, 255, 0, 255);

    draw_list->AddRectFilled(ImVec2(crop_min.x - handle_size/2, crop_min.y - handle_size/2),
                            ImVec2(crop_min.x + handle_size/2, crop_min.y + handle_size/2), handle_color);
    draw_list->AddRectFilled(ImVec2(crop_max.x - handle_size/2, crop_min.y - handle_size/2),
                            ImVec2(crop_max.x + handle_size/2, crop_min.y + handle_size/2), handle_color);
    draw_list->AddRectFilled(ImVec2(crop_min.x - handle_size/2, crop_max.y - handle_size/2),
                            ImVec2(crop_min.x + handle_size/2, crop_max.y + handle_size/2), handle_color);
    draw_list->AddRectFilled(ImVec2(crop_max.x - handle_size/2, crop_max.y - handle_size/2),
                            ImVec2(crop_max.x + handle_size/2, crop_max.y + handle_size/2), handle_color);

    // Draw edge handles
    ImVec2 center_top((crop_min.x + crop_max.x) * 0.5f, crop_min.y);
    ImVec2 center_bottom((crop_min.x + crop_max.x) * 0.5f, crop_max.y);
    ImVec2 center_left(crop_min.x, (crop_min.y + crop_max.y) * 0.5f);
    ImVec2 center_right(crop_max.x, (crop_min.y + crop_max.y) * 0.5f);

    draw_list->AddRectFilled(ImVec2(center_top.x - handle_size/2, center_top.y - handle_size/2),
                            ImVec2(center_top.x + handle_size/2, center_top.y + handle_size/2), handle_color);
    draw_list->AddRectFilled(ImVec2(center_bottom.x - handle_size/2, center_bottom.y - handle_size/2),
                            ImVec2(center_bottom.x + handle_size/2, center_bottom.y + handle_size/2), handle_color);
    draw_list->AddRectFilled(ImVec2(center_left.x - handle_size/2, center_left.y - handle_size/2),
                            ImVec2(center_left.x + handle_size/2, center_left.y + handle_size/2), handle_color);
    draw_list->AddRectFilled(ImVec2(center_right.x - handle_size/2, center_right.y - handle_size/2),
                            ImVec2(center_right.x + handle_size/2, center_right.y + handle_size/2), handle_color);

    // Draw crop dimensions text on top of the crop rectangle
    char dimensions_text[128];
    snprintf(dimensions_text, sizeof(dimensions_text), "%d x %d", preview.crop_w, preview.crop_h);

    ImVec2 text_size = ImGui::CalcTextSize(dimensions_text);
    ImVec2 text_pos(
        (crop_min.x + crop_max.x - text_size.x) * 0.5f,
        crop_min.y + 10.0f  // 10 pixels from top of crop rect
    );

    // Draw background for text
    float text_padding = 4.0f;
    draw_list->AddRectFilled(
        ImVec2(text_pos.x - text_padding, text_pos.y - text_padding),
        ImVec2(text_pos.x + text_size.x + text_padding, text_pos.y + text_size.y + text_padding),
        IM_COL32(0, 0, 0, 200)
    );

    // Draw text
    draw_list->AddText(text_pos, IM_COL32(255, 255, 0, 255), dimensions_text);

    // Handle interaction
    HandleCropInteraction(preview, display_pos, display_size, scale);
}

void CameraPreviewWidget::HandleCropInteraction(CameraPreview& preview, const ImVec2& display_pos, const ImVec2& display_size, float scale) {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mouse_pos = io.MousePos;

    // Convert crop rect to screen coordinates
    ImVec2 crop_min(
        display_pos.x + preview.crop_x / scale,
        display_pos.y + preview.crop_y / scale
    );
    ImVec2 crop_max(
        display_pos.x + (preview.crop_x + preview.crop_w) / scale,
        display_pos.y + (preview.crop_y + preview.crop_h) / scale
    );

    float handle_size = 12.0f;

    // Start drag
    if (ImGui::IsMouseClicked(0) && !preview.is_dragging_crop) {
        // Check if mouse is over crop area
        if (mouse_pos.x >= display_pos.x && mouse_pos.x <= display_pos.x + display_size.x &&
            mouse_pos.y >= display_pos.y && mouse_pos.y <= display_pos.y + display_size.y) {

            preview.drag_mode = GetDragMode(preview, mouse_pos, crop_min, crop_max, handle_size);

            if (preview.drag_mode != CameraPreview::None) {
                preview.is_dragging_crop = true;
                preview.drag_start_pos = mouse_pos;
                preview.drag_start_crop_x = preview.crop_x;
                preview.drag_start_crop_y = preview.crop_y;
                preview.drag_start_crop_w = preview.crop_w;
                preview.drag_start_crop_h = preview.crop_h;
            }
        }
    }

    // During drag
    if (preview.is_dragging_crop && ImGui::IsMouseDragging(0)) {
        ImVec2 mouse_delta(
            mouse_pos.x - preview.drag_start_pos.x,
            mouse_pos.y - preview.drag_start_pos.y
        );
        UpdateCropFromDrag(preview, mouse_delta, scale);
    }

    // End drag
    if (ImGui::IsMouseReleased(0) && preview.is_dragging_crop) {
        preview.is_dragging_crop = false;
        preview.drag_mode = CameraPreview::None;
        SaveCropSettings();
    }
}

CameraPreview::DragMode CameraPreviewWidget::GetDragMode(const CameraPreview& preview, const ImVec2& mouse_pos,
                                                         const ImVec2& crop_min, const ImVec2& crop_max, float handle_size) {
    // Check corner handles first
    if (std::abs(mouse_pos.x - crop_min.x) < handle_size && std::abs(mouse_pos.y - crop_min.y) < handle_size)
        return CameraPreview::ResizeTopLeft;
    if (std::abs(mouse_pos.x - crop_max.x) < handle_size && std::abs(mouse_pos.y - crop_min.y) < handle_size)
        return CameraPreview::ResizeTopRight;
    if (std::abs(mouse_pos.x - crop_min.x) < handle_size && std::abs(mouse_pos.y - crop_max.y) < handle_size)
        return CameraPreview::ResizeBottomLeft;
    if (std::abs(mouse_pos.x - crop_max.x) < handle_size && std::abs(mouse_pos.y - crop_max.y) < handle_size)
        return CameraPreview::ResizeBottomRight;

    // Check edge handles
    float center_x = (crop_min.x + crop_max.x) * 0.5f;
    float center_y = (crop_min.y + crop_max.y) * 0.5f;

    if (std::abs(mouse_pos.x - center_x) < handle_size && std::abs(mouse_pos.y - crop_min.y) < handle_size)
        return CameraPreview::ResizeTop;
    if (std::abs(mouse_pos.x - center_x) < handle_size && std::abs(mouse_pos.y - crop_max.y) < handle_size)
        return CameraPreview::ResizeBottom;
    if (std::abs(mouse_pos.x - crop_min.x) < handle_size && std::abs(mouse_pos.y - center_y) < handle_size)
        return CameraPreview::ResizeLeft;
    if (std::abs(mouse_pos.x - crop_max.x) < handle_size && std::abs(mouse_pos.y - center_y) < handle_size)
        return CameraPreview::ResizeRight;

    // Check if inside crop rect (move)
    if (mouse_pos.x >= crop_min.x && mouse_pos.x <= crop_max.x &&
        mouse_pos.y >= crop_min.y && mouse_pos.y <= crop_max.y)
        return CameraPreview::Move;

    return CameraPreview::None;
}

void CameraPreviewWidget::UpdateCropFromDrag(CameraPreview& preview, const ImVec2& mouse_delta, float scale) {
    int delta_x = (int)(mouse_delta.x * scale);
    int delta_y = (int)(mouse_delta.y * scale);

    switch (preview.drag_mode) {
        case CameraPreview::Move:
            preview.crop_x = preview.drag_start_crop_x + delta_x;
            preview.crop_y = preview.drag_start_crop_y + delta_y;
            break;

        case CameraPreview::ResizeTopLeft:
            preview.crop_x = preview.drag_start_crop_x + delta_x;
            preview.crop_y = preview.drag_start_crop_y + delta_y;
            preview.crop_w = preview.drag_start_crop_w - delta_x;
            preview.crop_h = preview.drag_start_crop_h - delta_y;
            break;

        case CameraPreview::ResizeTopRight:
            preview.crop_y = preview.drag_start_crop_y + delta_y;
            preview.crop_w = preview.drag_start_crop_w + delta_x;
            preview.crop_h = preview.drag_start_crop_h - delta_y;
            break;

        case CameraPreview::ResizeBottomLeft:
            preview.crop_x = preview.drag_start_crop_x + delta_x;
            preview.crop_w = preview.drag_start_crop_w - delta_x;
            preview.crop_h = preview.drag_start_crop_h + delta_y;
            break;

        case CameraPreview::ResizeBottomRight:
            preview.crop_w = preview.drag_start_crop_w + delta_x;
            preview.crop_h = preview.drag_start_crop_h + delta_y;
            break;

        case CameraPreview::ResizeTop:
            preview.crop_y = preview.drag_start_crop_y + delta_y;
            preview.crop_h = preview.drag_start_crop_h - delta_y;
            if (maintain_aspect_ratio_) {
                preview.crop_w = (int)(preview.crop_h * global_aspect_ratio_);
            }
            break;

        case CameraPreview::ResizeBottom:
            preview.crop_h = preview.drag_start_crop_h + delta_y;
            if (maintain_aspect_ratio_) {
                preview.crop_w = (int)(preview.crop_h * global_aspect_ratio_);
            }
            break;

        case CameraPreview::ResizeLeft:
            preview.crop_x = preview.drag_start_crop_x + delta_x;
            preview.crop_w = preview.drag_start_crop_w - delta_x;
            if (maintain_aspect_ratio_) {
                preview.crop_h = (int)(preview.crop_w / global_aspect_ratio_);
            }
            break;

        case CameraPreview::ResizeRight:
            preview.crop_w = preview.drag_start_crop_w + delta_x;
            if (maintain_aspect_ratio_) {
                preview.crop_h = (int)(preview.crop_w / global_aspect_ratio_);
            }
            break;
    }

    ClampCropRect(preview);
}

void CameraPreviewWidget::ClampCropRect(CameraPreview& preview) {
    // Ensure minimum size
    const int min_size = 100;
    if (preview.crop_w < min_size) preview.crop_w = min_size;
    if (preview.crop_h < min_size) preview.crop_h = min_size;

    // Ensure maximum size
    if (preview.crop_w > preview.image_width) preview.crop_w = preview.image_width;
    if (preview.crop_h > preview.image_height) preview.crop_h = preview.image_height;

    // Ensure position is within bounds
    if (preview.crop_x < 0) preview.crop_x = 0;
    if (preview.crop_y < 0) preview.crop_y = 0;
    if (preview.crop_x + preview.crop_w > preview.image_width)
        preview.crop_x = preview.image_width - preview.crop_w;
    if (preview.crop_y + preview.crop_h > preview.image_height)
        preview.crop_y = preview.image_height - preview.crop_h;
}

void CameraPreviewWidget::CapturePreviewImages() {
    if (!camera_manager_ || is_capturing_) return;

    is_capturing_ = true;
    LogMessage("Starting preview capture...");

    // Create temp directory for preview images
    std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "neural_capture_preview";
    std::filesystem::create_directories(temp_dir);
    std::string temp_dir_str = temp_dir.string();

    // Capture image from each camera
    for (auto& preview : camera_previews_) {
        if (!preview.camera_id.empty()) {
            LogMessage("Capturing preview from " + preview.camera_name + "...");

            // CaptureCamera expects a directory path, not a file path
            // It will generate its own filename inside that directory
            if (camera_manager_->CaptureCamera(preview.camera_id, temp_dir_str)) {
                // Find the most recently created file in the temp directory
                std::filesystem::path latest_file;
                std::filesystem::file_time_type latest_time;
                bool found = false;

                for (const auto& entry : std::filesystem::directory_iterator(temp_dir)) {
                    if (entry.is_regular_file()) {
                        auto ftime = std::filesystem::last_write_time(entry);
                        if (!found || ftime > latest_time) {
                            latest_file = entry.path();
                            latest_time = ftime;
                            found = true;
                        }
                    }
                }

                if (found) {
                    // Load the captured image
                    if (LoadPreviewImage(preview, latest_file.string())) {
                        LogMessage("✓ Preview loaded for " + preview.camera_name);
                    } else {
                        LogMessage("⚠ Failed to load preview for " + preview.camera_name);
                    }
                } else {
                    LogMessage("⚠ Could not find captured file for " + preview.camera_name);
                }
            } else {
                LogMessage("⚠ Failed to capture preview from " + preview.camera_name);
            }
        }
    }

    is_capturing_ = false;
    LogMessage("Preview capture completed");
}

void CameraPreviewWidget::ResetAllCrops() {
    for (auto& preview : camera_previews_) {
        preview.crop_x = 0;
        preview.crop_y = 0;
        preview.crop_w = preview.image_width > 0 ? preview.image_width : 4112;
        preview.crop_h = preview.image_height > 0 ? preview.image_height : 3008;
    }
    SaveCropSettings();
    LogMessage("Reset all crop rectangles");
}

void CameraPreviewWidget::ApplyCropToAllCameras() {
    if (camera_previews_.empty() || current_camera_index_ < 0 || current_camera_index_ >= camera_previews_.size()) {
        return;
    }

    // Get current camera's crop settings
    const auto& current = camera_previews_[current_camera_index_];

    // Apply to all cameras
    for (auto& preview : camera_previews_) {
        preview.crop_x = current.crop_x;
        preview.crop_y = current.crop_y;
        preview.crop_w = current.crop_w;
        preview.crop_h = current.crop_h;
    }

    SaveCropSettings();
    LogMessage("Applied crop settings from " + current.camera_name + " to all cameras");
}

void CameraPreviewWidget::UpdateCameraPreviews() {
    if (!camera_manager_) return;

    camera_previews_.clear();

    auto cameras = camera_manager_->GetDiscoveredCameras();
    for (const auto& cam : cameras) {
        CameraPreview preview;
        preview.camera_id = cam.id;
        preview.camera_name = cam.name;
        preview.image_width = 4112;
        preview.image_height = 3008;
        preview.crop_x = 0;
        preview.crop_y = 0;
        preview.crop_w = 4112;
        preview.crop_h = 3008;

        camera_previews_.push_back(preview);
    }
}

void CameraPreviewWidget::ClearPreviews() {
    for (auto& preview : camera_previews_) {
        UnloadPreviewTexture(preview);
    }
    camera_previews_.clear();
}

bool CameraPreviewWidget::LoadPreviewImage(CameraPreview& preview, const std::string& image_path) {
#ifdef _WIN32
    if (!gdiplus_initialized_) return false;

    // Convert to wide string
    int len = MultiByteToWideChar(CP_UTF8, 0, image_path.c_str(), -1, nullptr, 0);
    std::wstring wpath(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, image_path.c_str(), -1, &wpath[0], len);

    // Load image with GDI+
    Bitmap* bitmap = Bitmap::FromFile(wpath.c_str());
    if (!bitmap || bitmap->GetLastStatus() != Ok) {
        delete bitmap;
        return false;
    }

    preview.image_width = bitmap->GetWidth();
    preview.image_height = bitmap->GetHeight();

    // Convert to RGBA
    BitmapData bitmapData;
    Rect rect(0, 0, preview.image_width, preview.image_height);
    bitmap->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &bitmapData);

    // Create OpenGL texture
    glGenTextures(1, &preview.texture_id);
    glBindTexture(GL_TEXTURE_2D, preview.texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    #ifndef GL_RGBA
    #define GL_RGBA 0x1908
    #endif
    #ifndef GL_BGRA_EXT
    #define GL_BGRA_EXT 0x80E1
    #endif

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, preview.image_width, preview.image_height,
                 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, bitmapData.Scan0);

    bitmap->UnlockBits(&bitmapData);
    delete bitmap;

    preview.has_preview = true;
    preview.preview_path = image_path;
    return true;
#else
    return false;
#endif
}

void CameraPreviewWidget::UnloadPreviewTexture(CameraPreview& preview) {
    if (preview.texture_id != 0) {
        glDeleteTextures(1, &preview.texture_id);
        preview.texture_id = 0;
    }
    preview.has_preview = false;
}

void CameraPreviewWidget::LoadCropSettings() {
    if (!settings_manager_) return;

    for (auto& preview : camera_previews_) {
        auto settings = settings_manager_->GetIndividualCameraSettings(preview.camera_id);
        preview.crop_x = settings.crop_offset_x;
        preview.crop_y = settings.crop_offset_y;
        preview.crop_w = settings.crop_width;
        preview.crop_h = settings.crop_height;
    }
}

void CameraPreviewWidget::SaveCropSettings() {
    if (!settings_manager_) return;

    for (const auto& preview : camera_previews_) {
        auto settings = settings_manager_->GetIndividualCameraSettings(preview.camera_id);
        settings.crop_offset_x = preview.crop_x;
        settings.crop_offset_y = preview.crop_y;
        settings.crop_width = preview.crop_w;
        settings.crop_height = preview.crop_h;
        settings_manager_->SetIndividualCameraSettings(preview.camera_id, settings);
    }
    settings_manager_->Save();
}

int CameraPreviewWidget::GetGridColumns() const {
    int count = camera_previews_.size();
    if (count <= 1) return 1;
    if (count <= 4) return 2;
    return 3;
}

int CameraPreviewWidget::GetGridRows() const {
    int count = camera_previews_.size();
    int columns = GetGridColumns();
    return (count + columns - 1) / columns;
}

void CameraPreviewWidget::LogMessage(const std::string& message) {
    if (log_callback_) {
        log_callback_(message);
    }
    AddGlobalLog(message, LogLevel::kInfo);
}

} // namespace SaperaCapturePro
