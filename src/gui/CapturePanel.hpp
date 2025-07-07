#pragma once

#include <functional>
#include <string>

struct CaptureSession;

class CapturePanel {
public:
    CapturePanel();
    ~CapturePanel();

    void Initialize();
    void Render(const CaptureSession& session);

    // Callbacks
    std::function<void()> onStartCapture;
    std::function<void()> onStopCapture;
    std::function<void()> onResetCapture;
    std::function<void(const std::string&)> onSetCaptureFormat;
    std::function<void(const std::string&)> onSetOutputPath;
    std::function<void(const std::string&)> onSetSessionName;

    bool visible = true;

private:
    // UI state
    char session_name_buffer_[256] = "";
    char output_path_buffer_[512] = "";
    int format_selection_ = 0;  // 0=TIFF, 1=RAW
    bool continuous_capture_ = false;
    int capture_interval_ms_ = 1000;
    int batch_size_ = 1;
    
    void RenderSessionControls(const CaptureSession& session);
    void RenderCaptureControls(const CaptureSession& session);
    void RenderFormatSettings();
    void RenderOutputSettings();
    void RenderBatchSettings();
}; 