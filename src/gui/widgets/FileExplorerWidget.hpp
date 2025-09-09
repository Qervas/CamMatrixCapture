#pragma once

#include <string>
#include <vector>
#include <functional>
#include <filesystem>
#include <imgui.h>
#include <glad/glad.h>
#include "../../utils/SessionManager.hpp"

namespace SaperaCapturePro {

struct FileInfo {
    std::filesystem::path path;
    std::string filename;
    std::string extension;
    size_t file_size = 0;
    bool is_image = false;
    
    FileInfo(const std::filesystem::path& p);
};

class FileExplorerWidget {
public:
    FileExplorerWidget();
    ~FileExplorerWidget();

    void Initialize();
    void Render(CaptureSession* session = nullptr);
    void Shutdown();

    // State queries
    std::string GetSelectedFilePath() const { return selected_file_path_; }
    bool HasSelection() const { return !selected_file_path_.empty(); }
    
    // Configuration
    void SetHeight(float height) { widget_height_ = height; }
    void SetShowPreview(bool show) { show_preview_ = show; }
    
    // Callbacks
    void SetOnFileSelected(std::function<void(const std::string&)> callback) {
        on_file_selected_ = callback;
    }
    
    void SetOnFileDoubleClick(std::function<void(const std::string&)> callback) {
        on_file_double_click_ = callback;
    }
    
    void SetLogCallback(std::function<void(const std::string&)> callback) {
        log_callback_ = callback;
    }

    // Actions
    void ClearSelection();
    void RefreshFiles();

private:
    // UI configuration
    float widget_height_ = 400.0f;
    bool show_preview_ = true;
    
    // State
    std::string selected_file_path_;
    std::vector<std::vector<FileInfo>> capture_files_; // Files per capture
    
    // Image preview state
    GLuint preview_texture_id_ = 0;
    int preview_width_ = 0;
    int preview_height_ = 0;
    bool has_valid_preview_ = false;
    std::string loaded_image_path_;
    
    // Callbacks
    std::function<void(const std::string&)> on_file_selected_;
    std::function<void(const std::string&)> on_file_double_click_;
    std::function<void(const std::string&)> log_callback_;
    
    // Helper methods
    void RenderFileTree(CaptureSession* session);
    void RenderPreview();
    void RenderActionButtons();
    void LoadFilesFromSession(CaptureSession* session);
    std::vector<FileInfo> GetFilesInDirectory(const std::filesystem::path& directory);
    bool IsImageFile(const std::string& extension) const;
    std::string FormatFileSize(size_t bytes) const;
    void LogMessage(const std::string& message);
    void OpenFileWithDefaultApp(const std::string& file_path);
    void ShowFileInExplorer(const std::string& file_path);
    
    // Image preview helpers
    bool LoadImagePreview(const std::string& image_path);
    void ClearImagePreview();
    bool LoadTIFFImage(const std::string& file_path, unsigned char*& data, int& width, int& height, int& channels);
};

} // namespace SaperaCapturePro