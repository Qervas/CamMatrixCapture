# GUI Module

ImGui-based user interface.

## Responsibilities

- Render main application window with docking
- Provide panels for hardware, bluetooth, capture, and logs
- Handle user input and coordinate module actions
- Display image previews
- Settings and preferences dialogs

## Key Classes

### Application

Main application class coordinating all GUI and backend systems.

**Interface:**
```cpp
Application();

bool Initialize();    // Setup all subsystems
void Run();          // Main render loop
void Shutdown();     // Cleanup
```

**Owns:**
- `GuiManager` - ImGui window and rendering
- `SettingsManager` - Persistent configuration
- `SessionManager` - Capture session organization
- `BluetoothManager*` - Singleton reference
- `CameraManager*` - Singleton reference

**GUI Panels:**
- `PreferencesDialog` - Application settings
- `LogPanel` - Message log display
- `HardwarePanel` - Camera control
- `CaptureStudioPanel` - Automated capture

**Window states:**
```cpp
bool show_preferences_;
bool show_hardware_panel_;
bool show_capture_studio_;
bool show_log_panel_;
bool show_session_manager_;
bool show_network_panel_;
```

### GuiManager

Handles ImGui initialization and window management.

**Responsibilities:**
- Create GLFW window
- Initialize ImGui with docking support
- Manage render loop (60 FPS)
- Handle window events (resize, close, etc.)

**Key methods:**
```cpp
bool Initialize(width, height, title);
void BeginFrame();      // Start ImGui frame
void EndFrame();        // Render and swap buffers
bool ShouldClose();     // Window close requested
void Shutdown();
```

### HardwarePanel

Camera discovery, connection, and parameter control.

**UI sections:**
- **Discovery**: Button to scan for cameras
- **Camera list**: Shows discovered cameras with status
- **Connection**: Connect/disconnect buttons
- **Parameters**: Exposure, gain, white balance, format controls
- **Capture**: Single capture button, sequential mode toggle

**Interaction with CameraManager:**
```cpp
if (ImGui::Button("Discover Cameras")) {
    CameraManager::GetInstance().DiscoverCameras([](msg) {
        LogPanel::AddLog(msg);
    });
}

if (ImGui::Button("Capture All")) {
    auto session_path = SessionManager::CreateSession();
    CameraManager::GetInstance().CaptureAllCameras(session_path, sequential, delay);
}
```

### CaptureStudioPanel

Automated multi-angle capture with turntable.

**UI sections:**
- **Hemisphere visualization**: 3D view of camera/turntable positions
- **Capture configuration**:
  - Number of rotation angles
  - Angle step size
  - Tilt angles (multi-layer capture)
  - Delays between steps
- **Session info**: Current session path, captured images count
- **Control buttons**: Start, Stop, Reset

**Automated capture flow:**
```cpp
// User clicks "Start Capture"
for (int tilt : tilt_angles) {
    BluetoothManager::TiltTurntable(device_id, tilt);
    std::this_thread::sleep_for(motion_delay);

    for (int step = 0; step < rotation_steps; ++step) {
        BluetoothManager::RotateTurntable(device_id, angle_per_step);
        std::this_thread::sleep_for(motion_delay);

        CameraManager::CaptureAllCameras(session_path);
        std::this_thread::sleep_for(capture_delay);

        // Update progress bar
        progress = (tilt_idx * rotation_steps + step) / total_captures;
    }
}

// Notify completion with sound
NotificationSound::Play(NotificationSound::Type::Success);
```

### LogPanel

Message log with filtering and export.

**Features:**
- Timestamp-prefixed messages
- Log levels (Info, Warning, Error)
- Auto-scroll
- Search/filter
- Export to file

**Usage:**
```cpp
LogPanel::AddLog("Camera discovery started");
LogPanel::AddLog("[ERROR] Failed to connect to camera 1");
```

### PreferencesDialog

Application-wide settings.

**Settings:**
- UI scale factor
- Default capture format (raw/color)
- Capture delays (sequential, motion)
- Output directory
- Bluetooth UUIDs
- Camera parameters defaults

**Persistence:**
```cpp
void PreferencesDialog::Apply() {
    SettingsManager::SetUIScale(ui_scale);
    SettingsManager::SetCaptureFormat(format);
    SettingsManager::SaveSettings();
}
```

### Widgets

#### CaptureStudioPanel
Main capture automation interface with hemisphere visualization.

#### SessionWidget
Browse and manage capture sessions.

**Features:**
- List all sessions by timestamp
- View session images
- Delete sessions
- Export metadata

#### FileExplorerWidget
File browser for navigating capture output.

**Features:**
- Directory tree view
- Image thumbnail preview
- File operations (rename, delete, copy)

## ImGui Integration

Uses **ImGui docking branch** for professional layout:

```cpp
// Docking example in Application::RenderDockSpace()
ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

if (first_time) {
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);

    ImGuiID dock_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f, nullptr, &dockspace_id);
    ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.30f, nullptr, &dockspace_id);

    ImGui::DockBuilderDockWindow("Hardware", dock_left);
    ImGui::DockBuilderDockWindow("Capture Studio", dockspace_id);
    ImGui::DockBuilderDockWindow("Log", dock_bottom);

    ImGui::DockBuilderFinish(dockspace_id);
}
```

**Window flags:**
```cpp
ImGuiWindowFlags window_flags =
    ImGuiWindowFlags_MenuBar |
    ImGuiWindowFlags_NoDocking;
```

**Rendering loop:**
```cpp
void Application::Run() {
    while (!gui_manager_->ShouldClose()) {
        gui_manager_->BeginFrame();

        RenderDockSpace();
        if (show_hardware_panel_) hardware_panel_->Render();
        if (show_capture_studio_) capture_studio_panel_->Render();
        if (show_log_panel_) log_panel_->Render();

        gui_manager_->EndFrame();
    }
}
```

## Theme and Styling

**Custom theme** applied in `GuiManager::Initialize()`:

```cpp
ImGuiStyle& style = ImGui::GetStyle();
style.WindowRounding = 5.0f;
style.FrameRounding = 3.0f;
style.GrabRounding = 3.0f;
style.ChildBorderSize = 1.0f;

ImVec4* colors = style.Colors;
colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
colors[ImGuiCol_Button] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
```

## Extension Points

### Adding New Panels

1. Create panel class:
```cpp
class MyNewPanel {
public:
    void Render();
private:
    bool show_ = true;
};
```

2. Add to `Application.hpp`:
```cpp
std::unique_ptr<MyNewPanel> my_new_panel_;
bool show_my_new_panel_ = true;
```

3. Initialize in `Application::InitializeGUI()`:
```cpp
my_new_panel_ = std::make_unique<MyNewPanel>();
```

4. Render in `Application::Run()`:
```cpp
if (show_my_new_panel_) my_new_panel_->Render();
```

5. Add menu item:
```cpp
if (ImGui::BeginMenu("View")) {
    ImGui::MenuItem("My New Panel", nullptr, &show_my_new_panel_);
    ImGui::EndMenu();
}
```

### Custom ImGui Widgets

Example: Color-coded status indicator
```cpp
void RenderStatusIndicator(CameraStatus status) {
    ImVec4 color;
    const char* text;

    switch (status) {
        case CameraStatus::Connected:
            color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
            text = "Connected";
            break;
        case CameraStatus::Disconnected:
            color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
            text = "Disconnected";
            break;
        default:
            color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
            text = "Unknown";
    }

    ImGui::TextColored(color, "%s", text);
}
```

### Image Preview

Load and display images as textures:

```cpp
bool Application::LoadImagePreview(const std::string& path) {
    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);

    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);

    preview_texture_id_ = texture_id;
    preview_width_ = width;
    preview_height_ = height;
    return true;
}

void RenderPreview() {
    ImGui::Image((void*)(intptr_t)preview_texture_id_,
                 ImVec2(preview_width_, preview_height_));
}
```

### Integrating New Hardware Modules

If you add a new module (e.g., lighting control):

1. Create `LightingPanel.hpp/cpp`
2. Add panel to `Application`
3. Call module functions from panel:
```cpp
void LightingPanel::Render() {
    ImGui::Begin("Lighting Control");

    if (ImGui::SliderFloat("Brightness", &brightness, 0.0f, 1.0f)) {
        LightingManager::GetInstance().SetBrightness(brightness);
    }

    ImGui::End();
}
```

## Layout and Docking

**Default layout:**
```
┌────────────────────────────────────────┐
│ Menu Bar                               │
├──────────┬─────────────────────────────┤
│          │                             │
│ Hardware │   Capture Studio            │
│          │   (with hemisphere view)    │
│          │                             │
├──────────┴─────────────────────────────┤
│ Log Panel                              │
└────────────────────────────────────────┘
```

**Customization:**
- Users can drag panels to rearrange
- Layout is saved in `imgui.ini` (auto-generated)
- Reset layout via menu: View → Reset Layout

## Performance Considerations

- **Target frame rate**: 60 FPS
- **Render only when needed**: Skip frames if no input/updates
- **Texture caching**: Reuse OpenGL textures for previews
- **Async operations**: Long tasks (capture, discovery) run on threads, UI polls status

## Common Patterns

**Modal dialog:**
```cpp
if (show_modal) {
    ImGui::OpenPopup("Confirm");
}

if (ImGui::BeginPopupModal("Confirm")) {
    ImGui::Text("Are you sure?");
    if (ImGui::Button("Yes")) {
        // Action
        ImGui::CloseCurrentPopup();
    }
    if (ImGui::Button("No")) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
```

**Progress bar:**
```cpp
float progress = (float)current_step / total_steps;
ImGui::ProgressBar(progress, ImVec2(-1, 0),
                   std::format("{}/{}", current_step, total_steps).c_str());
```

**Tooltip:**
```cpp
ImGui::Button("?");
if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("This is a helpful tooltip");
}
```
