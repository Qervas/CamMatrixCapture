#include "FileExplorerWidget.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#ifdef _WIN32
#include <windows.h>
#include <objidl.h>      // For IStream
#include <wtypes.h>      // For PROPID
#include <wingdi.h>      // For HDC
#include <comdef.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#endif

namespace SaperaCapturePro {

FileInfo::FileInfo(const std::filesystem::path& p) : path(p) {
    filename = p.filename().string();
    extension = p.extension().string();
    
    // Convert extension to lowercase
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    // Get file size
    std::error_code ec;
    file_size = std::filesystem::file_size(p, ec);
    if (ec) file_size = 0;
    
    // Check if it's an image file
    is_image = (extension == ".tiff" || extension == ".tif" || extension == ".raw" || 
                extension == ".png" || extension == ".jpg" || extension == ".jpeg");
}

FileExplorerWidget::FileExplorerWidget() = default;

FileExplorerWidget::~FileExplorerWidget() {
    Shutdown();
}

void FileExplorerWidget::Initialize() {
#ifdef _WIN32
    // Initialize GDI+ for TIFF loading
    if (!gdiplus_initialized_) {
        using namespace Gdiplus;
        GdiplusStartupInput gdiplusStartupInput;
        Status status = GdiplusStartup(&gdiplus_token_, &gdiplusStartupInput, nullptr);
        if (status == Ok) {
            gdiplus_initialized_ = true;
            LogMessage("[FILE] GDI+ initialized successfully");
        } else {
            LogMessage("[FILE] Failed to initialize GDI+: " + std::to_string(status));
        }
    }
#endif
}

void FileExplorerWidget::Shutdown() {
    ClearSelection();
    ClearImagePreview();
    
#ifdef _WIN32
    // Shutdown GDI+ if it was initialized
    if (gdiplus_initialized_) {
        Gdiplus::GdiplusShutdown(gdiplus_token_);
        gdiplus_initialized_ = false;
        LogMessage("[FILE] GDI+ shutdown complete");
    }
#endif
}

void FileExplorerWidget::Render(CaptureSession* session) {
    // Ensure initialization is complete
#ifdef _WIN32
    if (!gdiplus_initialized_) {
        Initialize();
    }
#endif
    
    if (ImGui::Begin("📁 File Explorer", nullptr, ImGuiWindowFlags_NoCollapse)) {
        
        // Header with actions
        if (ImGui::SmallButton("🔄 Refresh")) {
            RefreshFiles();
        }
        ImGui::SameLine();
        if (HasSelection()) {
            if (ImGui::SmallButton("❌ Clear")) {
                ClearSelection();
            }
        }
        
        ImGui::Separator();
        
        if (!session) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No active session - start a session to view files");
        } else {
            LoadFilesFromSession(session);
            
            // Two-panel layout: File tree on left, Preview on right
            ImVec2 available_size = ImGui::GetContentRegionAvail();
            
            // File tree panel (left side, 40% width)
            ImGui::BeginChild("FileTreePanel", ImVec2(available_size.x * 0.4f, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::Text("📂 Files");
            ImGui::Separator();
            RenderFileTree(session);
            ImGui::EndChild();
            
            ImGui::SameLine();
            
            // Preview panel (right side, remaining width)  
            ImGui::BeginChild("PreviewPanel", ImVec2(0, 0), true);
            ImGui::Text("🔍 Preview");
            ImGui::Separator();
            RenderPreview();
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

void FileExplorerWidget::RenderFileTree(CaptureSession* session) {
    if (capture_files_.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No captures yet - take your first capture!");
        return;
    }
    
    ImGui::BeginChild("FileTree", ImVec2(0, show_preview_ && HasSelection() ? 0 : -30), false);
    
    for (size_t i = 0; i < capture_files_.size(); i++) {
        const auto& files = capture_files_[i];
        if (files.empty()) continue;
        
        // Use the capture directory name for the tree node
        std::filesystem::path capture_dir = files[0].path.parent_path();
        std::string tree_label = "Capture " + std::to_string(i + 1) + " - " + capture_dir.filename().string();
        
        if (ImGui::TreeNode(tree_label.c_str())) {
            for (const auto& file : files) {
                bool is_selected = (file.path.string() == selected_file_path_);
                
                ImGuiSelectableFlags flags = ImGuiSelectableFlags_None;
                if (ImGui::Selectable(file.filename.c_str(), is_selected, flags)) {
                    selected_file_path_ = file.path.string();
                    
                    // Load image preview if it's a new selection
                    if (file.is_image && selected_file_path_ != loaded_image_path_) {
                        LoadImagePreview(selected_file_path_);
                    }
                    
                    if (on_file_selected_) {
                        on_file_selected_(selected_file_path_);
                    }
                }
                
                // Double-click to open
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    if (on_file_double_click_) {
                        on_file_double_click_(file.path.string());
                    } else {
                        OpenFileWithDefaultApp(file.path.string());
                    }
                }
                
                // Hover tooltip with file info
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("File: %s", file.filename.c_str());
                    ImGui::Text("Size: %s", FormatFileSize(file.file_size).c_str());
                    ImGui::Text("Path: %s", file.path.string().c_str());
                    if (file.is_image) {
                        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "📷 Image file");
                    }
                    ImGui::EndTooltip();
                }
            }
            ImGui::TreePop();
        }
    }
    
    ImGui::EndChild();
    
    // Action buttons at bottom (if not showing preview)
    if (!show_preview_ || !HasSelection()) {
        RenderActionButtons();
    }
}

void FileExplorerWidget::RenderPreview() {
    if (!HasSelection()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Select a file to preview");
        return;
    }
    
    std::filesystem::path selected_path(selected_file_path_);
    FileInfo file_info(selected_path);
    
    // Compact header with file info in one line
    ImGui::Text("📄 %s", file_info.filename.c_str());
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "(%s)", FormatFileSize(file_info.file_size).c_str());
    
    // Action buttons in compact row
    if (ImGui::SmallButton("🔍 Explorer")) {
        ShowFileInExplorer(selected_file_path_);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("📱 Open App")) {
        OpenFileWithDefaultApp(selected_file_path_);
    }
    
    ImGui::Separator();
    
    if (file_info.is_image && has_valid_preview_ && preview_texture_id_ != 0) {
        
        // Get full available space for image
        ImVec2 available_size = ImGui::GetContentRegionAvail();
        
        float aspect_ratio = static_cast<float>(preview_width_) / static_cast<float>(preview_height_);
        ImVec2 display_size;
        
        // Scale to fit available space while maintaining aspect ratio
        if (available_size.x / aspect_ratio <= available_size.y) {
            display_size.x = available_size.x - 20; // Leave margin
            display_size.y = display_size.x / aspect_ratio;
        } else {
            display_size.y = available_size.y - 40; // Leave margin for text
            display_size.x = display_size.y * aspect_ratio;
        }
        
        // Ensure minimum reasonable size
        if (display_size.x < 100) {
            display_size.x = 100;
            display_size.y = 100 / aspect_ratio;
        }
        
        // Center the image in available space
        ImVec2 cursor_pos = ImGui::GetCursorPos();
        ImVec2 centered_pos = ImVec2(
            cursor_pos.x + std::max(0.0f, (available_size.x - display_size.x) * 0.5f),
            cursor_pos.y + std::max(0.0f, (available_size.y - display_size.y - 30) * 0.5f)
        );
        ImGui::SetCursorPos(centered_pos);
        
        // Render the image
        ImGui::Image((ImTextureID)(uintptr_t)preview_texture_id_, display_size);
        
        // Show dimensions on hover
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Image: %dx%d pixels", preview_width_, preview_height_);
            ImGui::Text("Texture ID: %u", preview_texture_id_);
            ImGui::Text("Display Size: %.0fx%.0f", display_size.x, display_size.y);
            ImGui::Text("Double-click to open with default app");
            ImGui::EndTooltip();
            
            if (ImGui::IsMouseDoubleClicked(0)) {
                OpenFileWithDefaultApp(selected_file_path_);
            }
        }
        
        // Show dimensions at bottom
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "📏 %dx%d pixels", preview_width_, preview_height_);
        
    } else if (file_info.is_image) {
        // Loading/error state
        LogMessage("[FILE] Image file selected but no preview - has_valid_preview_: " + 
                   std::to_string(has_valid_preview_) + " texture_id: " + std::to_string(preview_texture_id_));
        
        ImVec2 available_size = ImGui::GetContentRegionAvail();
        ImVec2 text_pos = ImVec2(
            available_size.x * 0.5f - 80,
            available_size.y * 0.5f - 20
        );
        ImGui::SetCursorPos(text_pos);
        
        if (loaded_image_path_ == selected_file_path_) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "❌ Failed to load image");
            ImGui::SetCursorPos(ImVec2(text_pos.x - 20, text_pos.y + 15));
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Check console for error details");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "⏳ Loading preview...");
        }
    } else {
        // Not an image file
        ImVec2 available_size = ImGui::GetContentRegionAvail();
        ImVec2 text_pos = ImVec2(
            available_size.x * 0.5f - 100,
            available_size.y * 0.5f - 10
        );
        ImGui::SetCursorPos(text_pos);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "📄 No preview available");
        
        text_pos.y += 20;
        ImGui::SetCursorPos(text_pos);
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Double-click to open");
    }
}

void FileExplorerWidget::RenderActionButtons() {
    if (!HasSelection()) return;
    
    if (ImGui::Button("🔍 Show in Explorer", ImVec2(140, 0))) {
        ShowFileInExplorer(selected_file_path_);
    }
    ImGui::SameLine();
    if (ImGui::Button("📱 Open with App", ImVec2(120, 0))) {
        OpenFileWithDefaultApp(selected_file_path_);
    }
}

void FileExplorerWidget::LoadFilesFromSession(CaptureSession* session) {
    capture_files_.clear();
    
    for (const auto& capture_path : session->capture_paths) {
        std::filesystem::path dir_path(capture_path);
        if (std::filesystem::exists(dir_path) && std::filesystem::is_directory(dir_path)) {
            auto files = GetFilesInDirectory(dir_path);
            if (!files.empty()) {
                capture_files_.push_back(files);
            }
        }
    }
}

std::vector<FileInfo> FileExplorerWidget::GetFilesInDirectory(const std::filesystem::path& directory) {
    std::vector<FileInfo> files;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                FileInfo file_info(entry.path());
                // Only include image files and some common formats
                if (file_info.is_image) {
                    files.push_back(file_info);
                }
            }
        }
        
        // Sort files by name
        std::sort(files.begin(), files.end(), [](const FileInfo& a, const FileInfo& b) {
            return a.filename < b.filename;
        });
        
    } catch (const std::exception& e) {
        LogMessage("[FILE] Error reading directory: " + directory.string() + " - " + e.what());
    }
    
    return files;
}

bool FileExplorerWidget::IsImageFile(const std::string& extension) const {
    std::string ext_lower = extension;
    std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), ::tolower);
    return (ext_lower == ".tiff" || ext_lower == ".tif" || ext_lower == ".raw" || 
            ext_lower == ".png" || ext_lower == ".jpg" || ext_lower == ".jpeg");
}

std::string FileExplorerWidget::FormatFileSize(size_t bytes) const {
    if (bytes > 1024 * 1024) {
        return std::to_string(bytes / (1024 * 1024)) + " MB";
    } else if (bytes > 1024) {
        return std::to_string(bytes / 1024) + " KB";
    } else {
        return std::to_string(bytes) + " bytes";
    }
}

void FileExplorerWidget::ClearSelection() {
    selected_file_path_.clear();
    if (on_file_selected_) {
        on_file_selected_("");
    }
}

void FileExplorerWidget::RefreshFiles() {
    LogMessage("[FILE] File list refreshed");
}

void FileExplorerWidget::LogMessage(const std::string& message) {
    if (log_callback_) {
        log_callback_(message);
    }
}

void FileExplorerWidget::OpenFileWithDefaultApp(const std::string& file_path) {
    std::string command = "start \"\" \"" + file_path + "\"";
    system(command.c_str());
    LogMessage("[FILE] Opening with default app: " + file_path);
}

void FileExplorerWidget::ShowFileInExplorer(const std::string& file_path) {
    std::filesystem::path abs_path = std::filesystem::absolute(file_path);
    std::string command = "explorer /select,\"" + abs_path.string() + "\"";
    system(command.c_str());
    LogMessage("[FILE] Showing in explorer: " + abs_path.string());
}

bool FileExplorerWidget::LoadImagePreview(const std::string& image_path) {
    // Clear previous preview
    ClearImagePreview();
    
    if (image_path.empty() || !std::filesystem::exists(image_path)) {
        return false;
    }
    
    LogMessage("[FILE] Attempting to load image: " + image_path);
    
    // Check file extension to determine loading method
    std::filesystem::path path(image_path);
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    int width, height, channels;
    unsigned char* data = nullptr;
    
    if (extension == ".tiff" || extension == ".tif") {
        // Use WIC for TIFF files
        if (!LoadTIFFImage(image_path, data, width, height, channels)) {
            LogMessage("[FILE] Failed to load TIFF image: " + image_path);
            return false;
        }
    } else {
        // Use stb_image for other formats
        data = stbi_load(image_path.c_str(), &width, &height, &channels, 0);
        if (!data) {
            std::string error = stbi_failure_reason() ? stbi_failure_reason() : "Unknown error";
            LogMessage("[FILE] Failed to load image: " + image_path + " - " + error);
            return false;
        }
    }
    
    LogMessage("[FILE] Successfully loaded image: " + std::to_string(width) + "x" + std::to_string(height) + " (" + std::to_string(channels) + " channels)");
    
    // Convert to RGBA if needed
    unsigned char* rgba_data = nullptr;
    if (channels == 3) {
        rgba_data = new unsigned char[width * height * 4];
        for (int i = 0; i < width * height; i++) {
            rgba_data[i * 4] = data[i * 3];     // R
            rgba_data[i * 4 + 1] = data[i * 3 + 1]; // G
            rgba_data[i * 4 + 2] = data[i * 3 + 2]; // B
            rgba_data[i * 4 + 3] = 255;         // A
        }
        // Free original data using appropriate method
        if (extension == ".tiff" || extension == ".tif") {
            delete[] data;  // WIC uses new[]
        } else {
            stbi_image_free(data);  // stb_image uses malloc
        }
        data = rgba_data;
        channels = 4;
    } else if (channels == 1) {
        rgba_data = new unsigned char[width * height * 4];
        for (int i = 0; i < width * height; i++) {
            rgba_data[i * 4] = data[i];     // R
            rgba_data[i * 4 + 1] = data[i]; // G
            rgba_data[i * 4 + 2] = data[i]; // B
            rgba_data[i * 4 + 3] = 255;     // A
        }
        // Free original data using appropriate method
        if (extension == ".tiff" || extension == ".tif") {
            delete[] data;  // WIC uses new[]
        } else {
            stbi_image_free(data);  // stb_image uses malloc
        }
        data = rgba_data;
        channels = 4;
    }
    
    // Create OpenGL texture
    glGenTextures(1, &preview_texture_id_);
    if (preview_texture_id_ == 0) {
        LogMessage("[FILE] Failed to generate OpenGL texture");
        delete[] data;
        return false;
    }
    
    glBindTexture(0x0DE1, preview_texture_id_); // GL_TEXTURE_2D
    
    // Set texture parameters for better quality
    glTexParameteri(0x0DE1, 0x2801, 0x2601); // GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR
    glTexParameteri(0x0DE1, 0x2800, 0x2601); // GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR  
    glTexParameteri(0x0DE1, 0x2802, 0x812F); // GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE
    glTexParameteri(0x0DE1, 0x2803, 0x812F); // GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE
    
    // Upload texture data - always RGBA since we convert above
    glTexImage2D(0x0DE1, 0, 0x1908, width, height, 0, 0x1908, 0x1401, data); // GL_TEXTURE_2D, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE
    
    // Note: Skipping OpenGL error check for compatibility
    
    glBindTexture(0x0DE1, 0); // GL_TEXTURE_2D
    
    // Clean up - after conversion, data is always RGBA allocated with new[]
    delete[] data;
    
    // Update state
    preview_width_ = width;
    preview_height_ = height;
    has_valid_preview_ = true;
    loaded_image_path_ = image_path;
    
    LogMessage("[FILE] Image preview loaded: " + image_path + " (" + std::to_string(width) + "x" + std::to_string(height) + ")");
    return true;
}

void FileExplorerWidget::ClearImagePreview() {
    if (preview_texture_id_ != 0) {
        glDeleteTextures(1, &preview_texture_id_);
        preview_texture_id_ = 0;
    }
    
    preview_width_ = 0;
    preview_height_ = 0;
    has_valid_preview_ = false;
    loaded_image_path_.clear();
}

bool FileExplorerWidget::LoadTIFFImage(const std::string& file_path, unsigned char*& data, int& width, int& height, int& channels) {
#ifdef _WIN32
    using namespace Gdiplus;
    
    // Check if GDI+ is initialized
    if (!gdiplus_initialized_) {
        LogMessage("[FILE] GDI+ not initialized for TIFF loading");
        return false;
    }
    
    // Convert path to wide string
    std::wstring wfile_path(file_path.begin(), file_path.end());
    
    // Load the TIFF using GDI+
    Bitmap* bitmap = new Bitmap(wfile_path.c_str());
    if (bitmap->GetLastStatus() != Ok) {
        LogMessage("[FILE] Failed to load TIFF with GDI+: " + std::to_string(bitmap->GetLastStatus()));
        delete bitmap;
        return false;
    }
    
    width = bitmap->GetWidth();
    height = bitmap->GetHeight();
    PixelFormat pixelFormat = bitmap->GetPixelFormat();
    
    LogMessage("[FILE] GDI+ TIFF loaded: " + std::to_string(width) + "x" + std::to_string(height) + ", format: " + std::to_string(pixelFormat));
    
    // For indexed/palette formats, we need to convert to RGB first
    Bitmap* rgbBitmap = nullptr;
    if (pixelFormat == PixelFormat8bppIndexed || 
        pixelFormat == PixelFormat4bppIndexed || 
        pixelFormat == PixelFormat1bppIndexed ||
        pixelFormat == PixelFormat16bppGrayScale) {
        
        LogMessage("[FILE] Converting indexed/grayscale TIFF to RGB format");
        
        // Create a new 24bpp RGB bitmap
        rgbBitmap = new Bitmap(width, height, PixelFormat24bppRGB);
        Graphics graphics(rgbBitmap);
        
        // Draw the indexed bitmap onto RGB bitmap (handles palette conversion)
        graphics.DrawImage(bitmap, 0, 0, width, height);
        
        delete bitmap;
        bitmap = rgbBitmap;
        pixelFormat = PixelFormat24bppRGB;
        
        LogMessage("[FILE] Converted to RGB format successfully");
    }

    // Lock bitmap bits for direct access  
    BitmapData bitmapData;
    Rect rect(0, 0, width, height);
    Status lockStatus = bitmap->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &bitmapData);
    if (lockStatus != Ok) {
        LogMessage("[FILE] Failed to lock bitmap bits: " + std::to_string(lockStatus));
        delete bitmap;
        return false;
    }
    
    // Copy pixel data from GDI+ bitmap to our buffer
    int totalPixels = width * height;
    data = new unsigned char[totalPixels * 4]; // RGBA format
    channels = 4;
    
    BYTE* srcPixels = static_cast<BYTE*>(bitmapData.Scan0);
    int srcStride = bitmapData.Stride;
    
    // First pass: find min/max values for normalization (camera TIFFs often have limited dynamic range)
    unsigned char minVal = 255, maxVal = 0;
    for (int y = 0; y < height; y++) {
        BYTE* srcRow = srcPixels + (y * srcStride);
        for (int x = 0; x < width; x++) {
            BYTE* srcPixel = srcRow + (x * 4);
            // Check RGB values (ignore alpha)
            for (int c = 0; c < 3; c++) {
                unsigned char val = srcPixel[c];
                if (val < minVal) minVal = val;
                if (val > maxVal) maxVal = val;
            }
        }
    }
    
    // Calculate normalization parameters
    float range = maxVal - minVal;
    bool needsNormalization = (range < 240 && range > 0); // Normalize if dynamic range is very limited (even 1-2 values)
    
    LogMessage("[FILE] TIFF dynamic range: " + std::to_string(minVal) + "-" + std::to_string(maxVal) + 
               " (range: " + std::to_string((int)range) + ") " + 
               (needsNormalization ? "- applying normalization" : "- using original values"));
    
    // Convert from ARGB to RGBA with optional normalization
    for (int y = 0; y < height; y++) {
        BYTE* srcRow = srcPixels + (y * srcStride);
        unsigned char* dstRow = data + (y * width * 4);
        
        for (int x = 0; x < width; x++) {
            BYTE* srcPixel = srcRow + (x * 4); // ARGB format
            unsigned char* dstPixel = dstRow + (x * 4); // RGBA format
            
            // GDI+ uses BGRA, we need RGBA with optional normalization
            unsigned char b = srcPixel[0];
            unsigned char g = srcPixel[1]; 
            unsigned char r = srcPixel[2];
            unsigned char a = srcPixel[3];
            
            if (needsNormalization && range > 0) {
                // Stretch limited dynamic range to full 0-255
                float fRange = static_cast<float>(range);
                dstPixel[0] = static_cast<unsigned char>((r - minVal) / fRange * 255.0f); // R
                dstPixel[1] = static_cast<unsigned char>((g - minVal) / fRange * 255.0f); // G
                dstPixel[2] = static_cast<unsigned char>((b - minVal) / fRange * 255.0f); // B
            } else {
                dstPixel[0] = r; // R
                dstPixel[1] = g; // G
                dstPixel[2] = b; // B
            }
            dstPixel[3] = 255; // Always opaque for preview
        }
    }
    
    // Unlock the bitmap
    bitmap->UnlockBits(&bitmapData);
    delete bitmap;
    
    LogMessage("[FILE] Successfully loaded TIFF using GDI+: " + std::to_string(width) + "x" + std::to_string(height));
    return true;
    
#else
    // Fallback to stb_image for non-Windows (won't work for TIFF)
    data = stbi_load(file_path.c_str(), &width, &height, &channels, 0);
    return data != nullptr;
#endif
}

} // namespace SaperaCapturePro