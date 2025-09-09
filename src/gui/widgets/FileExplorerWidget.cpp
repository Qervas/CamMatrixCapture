#include "FileExplorerWidget.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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
    // Initialize any resources if needed
}

void FileExplorerWidget::Shutdown() {
    ClearSelection();
    ClearImagePreview();
}

void FileExplorerWidget::Render(CaptureSession* session) {
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
            display_size.x = available_size.x;
            display_size.y = available_size.x / aspect_ratio;
        } else {
            display_size.y = available_size.y;
            display_size.x = available_size.y * aspect_ratio;
        }
        
        // Center the image in available space
        ImVec2 cursor_pos = ImGui::GetCursorPos();
        ImVec2 centered_pos = ImVec2(
            cursor_pos.x + (available_size.x - display_size.x) * 0.5f,
            cursor_pos.y + (available_size.y - display_size.y) * 0.5f
        );
        ImGui::SetCursorPos(centered_pos);
        
        // Render the image
        ImGui::Image((ImTextureID)(uintptr_t)preview_texture_id_, display_size);
        
        // Show dimensions on hover
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Image: %dx%d pixels", preview_width_, preview_height_);
            ImGui::Text("Double-click to open with default app");
            ImGui::EndTooltip();
            
            if (ImGui::IsMouseDoubleClicked(0)) {
                OpenFileWithDefaultApp(selected_file_path_);
            }
        }
        
        // Show dimensions at bottom
        ImGui::SetCursorPos(ImVec2(10, available_size.y - 20));
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%dx%d", preview_width_, preview_height_);
        
    } else if (file_info.is_image) {
        // Loading state
        ImVec2 available_size = ImGui::GetContentRegionAvail();
        ImVec2 text_pos = ImVec2(
            available_size.x * 0.5f - 50,
            available_size.y * 0.5f - 10
        );
        ImGui::SetCursorPos(text_pos);
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "⏳ Loading...");
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
    
    // Load image using stb_image with TIFF support
    int width, height, channels;
    unsigned char* data = stbi_load(image_path.c_str(), &width, &height, &channels, 0);
    
    if (!data) {
        std::string error = stbi_failure_reason() ? stbi_failure_reason() : "Unknown error";
        LogMessage("[FILE] Failed to load image: " + image_path + " - " + error);
        return false;
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
        stbi_image_free(data);
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
        stbi_image_free(data);
        data = rgba_data;
        channels = 4;
    }
    
    // Create OpenGL texture
    glGenTextures(1, &preview_texture_id_);
    glBindTexture(0x0DE1, preview_texture_id_); // GL_TEXTURE_2D
    
    glTexParameteri(0x0DE1, 0x2601, 0x2601); // GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR
    glTexParameteri(0x0DE1, 0x2800, 0x2601); // GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR  
    glTexParameteri(0x0DE1, 0x2802, 0x2901); // GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT
    glTexParameteri(0x0DE1, 0x2803, 0x2901); // GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT
    
    GLenum format = (channels == 4) ? 0x1908 : 0x1907; // GL_RGBA : GL_RGB
    glTexImage2D(0x0DE1, 0, format, width, height, 0, format, 0x1401, data); // GL_TEXTURE_2D, GL_UNSIGNED_BYTE
    
    glBindTexture(0x0DE1, 0); // GL_TEXTURE_2D
    
    // Clean up
    if (channels == 4 && rgba_data) {
        delete[] data;
    } else {
        stbi_image_free(data);
    }
    
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
    // For now, use stb_image for TIFF loading
    data = stbi_load(file_path.c_str(), &width, &height, &channels, 0);
    return data != nullptr;
}

} // namespace SaperaCapturePro