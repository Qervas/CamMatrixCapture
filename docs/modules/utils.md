# Utils Module

Settings persistence, session management, and helper utilities.

## Key Classes

### SessionManager

Organizes capture sessions with timestamps and metadata.

**Interface:**
```cpp
SessionManager(output_path);

// Session control
bool StartNewSession(object_name);
void EndCurrentSession();
bool HasActiveSession();
CaptureSession* GetCurrentSession();

// Capture tracking
bool RecordCapture(capture_path);
std::string GetNextCapturePath();
int GetTotalCapturesInSession();

// History
const std::vector<CaptureSession>& GetSessionHistory();
int GetTotalSessions();

// Configuration
void SetOutputPath(path);
std::string GetOutputPath();
```

**CaptureSession struct:**
```cpp
struct CaptureSession {
    std::string object_name;
    std::string session_id;               // Timestamp format: 20250101_143052_456
    std::string base_path;                // Full path to session directory
    int capture_count;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_capture;
    std::vector<std::string> capture_paths;

    std::string GetNextCapturePath();
    std::string GetSessionInfo();
    SimpleJSON ToJson();
    void FromJson(const SimpleJSON& json);
};
```

**Directory structure:**
```
neural_dataset/
└── images/
    ├── 20250101_143052_456/
    │   ├── camera1_capture_001.tiff
    │   ├── camera1_capture_002.tiff
    │   ├── camera2_capture_001.tiff
    │   ├── camera2_capture_002.tiff
    │   └── session_metadata.json
    ├── 20250101_150230_789/
    │   └── ...
    └── sessions.json                  // Session history
```

**Usage example:**
```cpp
auto session_manager = std::make_unique<SessionManager>("neural_dataset/images");

// Start new session
session_manager->StartNewSession("vase_ceramic_blue");

// During automated capture
for (int i = 0; i < 24; ++i) {
    auto capture_path = session_manager->GetNextCapturePath();
    // capture_path = "neural_dataset/images/20250101_143052_456"

    camera_manager->CaptureAllCameras(capture_path, true, 750);
    session_manager->RecordCapture(capture_path);
}

// End session (saves metadata)
session_manager->EndCurrentSession();
```

**Metadata format (session_metadata.json):**
```json
{
    "object_name": "vase_ceramic_blue",
    "session_id": "20250101_143052_456",
    "capture_count": "48",
    "created_at": "2025-01-01T14:30:52",
    "last_capture": "2025-01-01T14:35:12"
}
```

### SettingsManager

Persistent application configuration (cameras, UI, bluetooth).

**Interface:**
```cpp
SettingsManager();

// General settings
void SetUIScale(scale);
float GetUIScale();
void SetOutputDirectory(path);
std::string GetOutputDirectory();

// Camera settings (per-camera)
void SaveCameraSettings(camera_id, parameters);
CameraManager::Parameters LoadCameraSettings(camera_id);

// Bluetooth settings
void SaveBluetoothDevice(device_id, device_name);
std::vector<std::pair<string, string>> GetSavedBluetoothDevices();

// Capture settings
void SetCaptureFormat(raw);
bool GetCaptureFormat();
void SetSequentialDelay(ms);
int GetSequentialDelay();

// Persistence
void SaveSettings();
void LoadSettings();
```

**Settings file (settings.ini in executable directory):**
```ini
# Application settings
app_ui_scale=1.2
app_last_output_folder=neural_dataset
app_dark_theme=true
app_window_width=1200
app_window_height=800

# Camera settings
camera_exposure_time=40000
camera_gain=1.5
camera_white_balance_red=1.2
camera_white_balance_green=1.0
camera_white_balance_blue=0.9
camera_pixel_format=RGB8

# Camera ordering
camera_order_use_custom_ordering=true
camera_order_order_entries=S1128470:0|S1128520:1|S1128522:2
```

**Note:** The settings.ini file is created in the same directory as the executable (e.g., `build/Debug/settings.ini` or `build/Release/settings.ini`), not in a config subfolder.

**Usage example:**
```cpp
auto settings = std::make_unique<SettingsManager>();

// Load settings on startup
settings->LoadSettings();
float ui_scale = settings->GetUIScale();

// Save camera parameters
CameraManager::Parameters params;
params.exposure_time = 50000;
params.gain = 2.0f;
settings->SaveCameraSettings("camera_12345678", params);

// Persist all settings
settings->SaveSettings();
```

### NotificationSounds

Audio feedback for capture events.

**Interface:**
```cpp
namespace NotificationSound {
    enum class Type {
        Success,    // Capture sequence completed
        Error,      // Operation failed
        Warning,    // Non-critical issue
        Info        // General notification
    };

    void Play(Type type);
    void SetEnabled(bool enabled);
    bool IsEnabled();
}
```

**Sound files:**
```
assets/sounds/
├── success.wav    // Pleasant chime
├── error.wav      // Alert beep
├── warning.wav    // Gentle notification
└── info.wav       // Neutral tone
```

**Usage example:**
```cpp
// On successful capture sequence
NotificationSound::Play(NotificationSound::Type::Success);

// On camera connection failure
NotificationSound::Play(NotificationSound::Type::Error);

// On bluetooth device disconnected
NotificationSound::Play(NotificationSound::Type::Warning);
```

**Implementation (Windows):**
```cpp
void Play(Type type) {
    if (!IsEnabled()) return;

    std::string sound_file;
    switch (type) {
        case Type::Success: sound_file = "assets/sounds/success.wav"; break;
        case Type::Error: sound_file = "assets/sounds/error.wav"; break;
        case Type::Warning: sound_file = "assets/sounds/warning.wav"; break;
        case Type::Info: sound_file = "assets/sounds/info.wav"; break;
    }

    // Windows API
    PlaySoundA(sound_file.c_str(), nullptr, SND_FILENAME | SND_ASYNC);
}
```

## Helper Utilities

### SimpleJSON

Lightweight JSON-like key-value store (no external dependencies).

**Usage:**
```cpp
SimpleJSON json;
json.set("name", "value");
json.set("count", 42);
json.set("temperature", 23.5f);
json.set("enabled", true);

std::string name = json.get("name", "default");
int count = json.getInt("count", 0);
float temp = json.getFloat("temperature", 0.0f);
bool enabled = json.getBool("enabled", false);
```

**Limitations:**
- No nested objects (flat key-value only)
- No arrays
- Basic type support (string, int, float, bool)

**When to use:**
- Simple configuration files
- Small metadata storage
- When nlohmann/json is overkill

### Filesystem Helpers

**Create timestamped directory:**
```cpp
std::string CreateTimestampedDirectory(const std::string& base_path) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S_")
       << std::setfill('0') << std::setw(3) << ms.count();

    std::filesystem::path full_path = base_path / ss.str();
    std::filesystem::create_directories(full_path);

    return full_path.string();
}
```

**List files in directory:**
```cpp
std::vector<std::string> ListFilesInDirectory(const std::string& dir_path, const std::string& extension = "") {
    std::vector<std::string> files;

    for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
        if (entry.is_regular_file()) {
            if (extension.empty() || entry.path().extension() == extension) {
                files.push_back(entry.path().string());
            }
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

// Usage
auto tiff_files = ListFilesInDirectory("session_path", ".tiff");
```

### String Utilities

**Format timestamp:**
```cpp
std::string FormatTimestamp(const std::chrono::system_clock::time_point& time) {
    auto time_t = std::chrono::system_clock::to_time_t(time);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}
```

**Parse camera ID:**
```cpp
std::string ExtractSerialNumber(const std::string& camera_id) {
    // camera_id: "GigE_Camera_12345678"
    auto pos = camera_id.find_last_of('_');
    if (pos != std::string::npos) {
        return camera_id.substr(pos + 1);
    }
    return camera_id;
}
```

## Extension Points

### Adding New Settings

1. Add getter/setter to `SettingsManager`:
```cpp
void SetMyNewSetting(int value) { my_new_setting_ = value; }
int GetMyNewSetting() const { return my_new_setting_; }
```

2. Save/load in `SaveSettings()` / `LoadSettings()`:
```cpp
void SaveSettings() {
    SimpleJSON json;
    json.set("my_new_setting", my_new_setting_);
    // Write to file...
}

void LoadSettings() {
    SimpleJSON json;
    // Load from file...
    my_new_setting_ = json.getInt("my_new_setting", default_value);
}
```

3. Add UI control in `PreferencesDialog`

### Custom Session Metadata

Add extra metadata to sessions:

```cpp
struct ExtendedCaptureSession : CaptureSession {
    std::string object_category;
    std::vector<std::string> tags;
    float object_height_cm;

    SimpleJSON ToJson() const override {
        auto json = CaptureSession::ToJson();
        json.set("object_category", object_category);
        json.set("object_height_cm", object_height_cm);
        // Tags as comma-separated
        json.set("tags", JoinStrings(tags, ","));
        return json;
    }
};
```

### Sound Customization

Replace sound files:

1. Place new WAV files in `assets/sounds/`
2. Ensure 16-bit PCM format
3. Recommended duration: 0.5-2 seconds

**Generate custom sounds:**
```python
# Python with pydub
from pydub import AudioSegment
from pydub.generators import Sine

# Generate 440Hz tone (A note)
tone = Sine(440).to_audio_segment(duration=500)  # 500ms
tone.export("success.wav", format="wav")
```

### Per-Session Notes

Add user notes to sessions:

```cpp
class SessionManager {
public:
    void AddNoteToSession(const std::string& note) {
        if (current_session) {
            current_session->notes.push_back({
                .timestamp = std::chrono::system_clock::now(),
                .text = note
            });
        }
    }
};

// Usage
session_manager->AddNoteToSession("Changed lighting setup");
session_manager->AddNoteToSession("Camera 2 had slight misalignment");
```

## Common Patterns

### Settings migration

Handle old settings format:

```cpp
void LoadSettings() {
    SimpleJSON json;
    // Load from file...

    // Check version
    int version = json.getInt("settings_version", 1);
    if (version < 2) {
        // Migrate old settings
        MigrateFromV1ToV2(json);
    }

    // Load current format
    ui_scale_ = json.getFloat("ui_scale", 1.0f);
}
```

### Session recovery

Recover incomplete sessions on startup:

```cpp
void RecoverIncompleteSessions() {
    for (auto& session : session_history) {
        if (session.status == "incomplete") {
            LogMessage("Found incomplete session: " + session.session_id);
            // Prompt user to delete or resume
        }
    }
}
```
