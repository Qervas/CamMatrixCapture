#include "NotificationSounds.hpp"
#define NOMINMAX
#include <Windows.h>
#include <mmsystem.h>
#include <filesystem>
#include <algorithm>
#include <string>

#pragma comment(lib, "winmm.lib")

namespace SaperaCapturePro {

NotificationSounds& NotificationSounds::Instance() {
    static NotificationSounds instance;
    return instance;
}

void NotificationSounds::Initialize() {
    if (initialized_) return;

    LogMessage("[SOUND] Initializing notification sound system...");
    initialized_ = true;
    LogMessage("[SOUND] Notification sound system ready");
}

void NotificationSounds::PlayCompletionSound() {
    if (!completion_sound_enabled_) {
        LogMessage("[SOUND] Completion sound disabled");
        return;
    }

    this->PlayNotificationSound(completion_sound_type_);
}

void NotificationSounds::PlayNotificationSound(SoundType type) {
    if (!initialized_) {
        Initialize();
    }

    std::string sound_name = GetSoundName(type);
    LogMessage("[SOUND] Playing sound: " + sound_name);

    switch (type) {
        case SoundType::WindowsDing:
            PlayWindowsSystemSound("SystemDefault");
            break;
        case SoundType::TrapBeat:
            PlaySoundFile("trap_beat.wav");
            break;
        case SoundType::MicrowaveDing:
            PlaySoundFile("microwave_ding.wav");
            break;
        case SoundType::CarSiren:
            PlaySoundFile("car_siren.wav");
            break;
        case SoundType::Custom:
            if (!custom_sound_path_.empty()) {
                PlaySoundFile(custom_sound_path_);
            } else {
                LogMessage("[SOUND] No custom sound file set, falling back to default");
                PlayDistinctBeep(800, 200); // Default beep
            }
            break;
    }
}

void NotificationSounds::PlaySoundFile(const std::string& filepath) {
    if (!std::filesystem::exists(filepath)) {
        LogMessage("[SOUND] ERROR: Sound file not found: " + filepath);
        // Fallback to system sound
        PlayWindowsSystemSound("SystemDefault");
        return;
    }

    // Convert to wide string for Windows API
    std::wstring wide_path(filepath.begin(), filepath.end());

    // Play sound asynchronously with volume (note: volume control is limited in PlaySound)
    DWORD flags = SND_FILENAME | SND_ASYNC | SND_NOWAIT;

    if (!PlaySoundW(wide_path.c_str(), NULL, flags)) {
        LogMessage("[SOUND] ERROR: Failed to play sound file: " + filepath);
        // Fallback to system sound
        PlayWindowsSystemSound("SystemDefault");
    } else {
        LogMessage("[SOUND] Playing custom sound: " + filepath);
    }
}

void NotificationSounds::TestSound(SoundType type, float volume) {
    float old_volume = notification_volume_;
    notification_volume_ = volume;

    LogMessage("[SOUND] Testing sound type: " + std::to_string(static_cast<int>(type)) + " with volume: " + std::to_string(volume * 100.0f) + "%");

    // For testing, force distinct beeps to ensure we hear differences
    PlayDistinctBeepForType(type);

    notification_volume_ = old_volume;
}

void NotificationSounds::PlayWindowsSystemSound(const std::string& sound_name) {
    // Convert to wide string for Windows API
    std::wstring wide_sound(sound_name.begin(), sound_name.end());

    // Play system sound asynchronously
    DWORD flags = SND_ALIAS | SND_ASYNC | SND_NOWAIT;

    if (!PlaySoundW(wide_sound.c_str(), NULL, flags)) {
        // Use distinct beep sounds for different types when system sounds fail
        if (sound_name == "SystemDefault") {
            PlayDistinctBeep(800, 200); // Standard beep
        } else if (sound_name == "SystemNotification") {
            PlayDistinctBeep(600, 150); // Lower pitch, shorter
        } else if (sound_name == "SystemExclamation") {
            PlayDistinctBeep(1000, 300); // Higher pitch, longer
        } else if (sound_name == "SystemAsterisk") {
            PlayDistinctBeep(1200, 100); // High pitch, short
            Sleep(50); // Brief pause
            PlayDistinctBeep(1200, 100); // Double beep
        } else if (sound_name == "SystemQuestion") {
            PlayDistinctBeep(400, 400); // Low pitch, long
        } else {
            PlayDistinctBeep(800, 200); // Default beep
        }
        LogMessage("[SOUND] Used distinct beep fallback for: " + sound_name);
    } else {
        LogMessage("[SOUND] Played system sound: " + sound_name);
    }
}

void NotificationSounds::PlayDistinctBeep(int frequency, int duration) {
    // Apply volume by adjusting both frequency and duration for more distinct differences
    float volume_factor = 0.2f + notification_volume_ * 0.8f; // 0.2 to 1.0 range
    int adjusted_frequency = static_cast<int>(frequency * volume_factor);
    int adjusted_duration = static_cast<int>(duration * volume_factor);

    // Ensure minimum values
    adjusted_frequency = std::max(200, adjusted_frequency);
    adjusted_duration = std::max(50, adjusted_duration);

    LogMessage("[SOUND] Playing beep: freq=" + std::to_string(adjusted_frequency) + "Hz, duration=" + std::to_string(adjusted_duration) + "ms");

    if (!Beep(adjusted_frequency, adjusted_duration)) {
        LogMessage("[SOUND] ERROR: Beep failed, using MessageBeep fallback");
        MessageBeep(MB_OK);
    }
}

void NotificationSounds::PlayDistinctBeepForType(SoundType type) {
    // Get the executable directory to construct full paths
    char exe_path[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    std::string exe_dir = std::filesystem::path(exe_path).parent_path().string();

    std::string sound_file;

    switch (type) {
        case SoundType::WindowsDing:
            sound_file = exe_dir + "\\sounds\\notification_ding.wav";
            break;
        case SoundType::TrapBeat:
            sound_file = exe_dir + "\\trap_beat.wav";
            break;
        case SoundType::MicrowaveDing:
            sound_file = exe_dir + "\\microwave_ding.wav";
            break;
        case SoundType::CarSiren:
            sound_file = exe_dir + "\\car_siren.wav";
            break;
        case SoundType::Custom:
            // Cycle through different hiphop beats for custom sound
            static int beat_index = 0;
            const std::vector<std::string> beats = {
                "trap_beat.wav",
                "boombap_beat.wav",
                "westcoast_beat.wav",
                "hardcore_beat.wav"
            };
            sound_file = exe_dir + "\\sounds\\" + beats[beat_index % beats.size()];
            beat_index++;
            break;
    }

    if (!sound_file.empty()) {
        PlaySoundFile(sound_file);
    } else {
        // Fallback to beep if file not found
        PlayDistinctBeep(800, 200);
    }
}

std::vector<std::pair<NotificationSounds::SoundType, std::string>> NotificationSounds::GetAvailableSounds() const {
    return {
        {SoundType::WindowsDing, "Windows Ding"},
        {SoundType::TrapBeat, "Trap Beat"},
        {SoundType::MicrowaveDing, "Microwave Ding (3x)"},
        {SoundType::CarSiren, "Car Siren"},
        {SoundType::Custom, "Custom Sound File"}
    };
}

std::string NotificationSounds::GetSoundName(SoundType type) const {
    switch (type) {
        case SoundType::WindowsDing: return "Windows Ding";
        case SoundType::TrapBeat: return "Trap Beat";
        case SoundType::MicrowaveDing: return "Microwave Ding (3x)";
        case SoundType::CarSiren: return "Car Siren";
        case SoundType::Custom: return "Custom Sound File";
        default: return "Unknown";
    }
}

void NotificationSounds::LogMessage(const std::string& message) {
    if (log_callback_) {
        log_callback_(message);
    }
}

} // namespace SaperaCapturePro