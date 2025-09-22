#pragma once

#include <string>
#include <vector>
#include <functional>

namespace SaperaCapturePro {

class NotificationSounds {
public:
    static NotificationSounds& Instance();

    // Available sound types
    enum class SoundType {
        WindowsDing,
        TrapBeat,
        MicrowaveDing,
        CarSiren,
        Custom
    };

    // Initialize sound system
    void Initialize();

    // Play completion sound with current settings
    void PlayCompletionSound();

    // Play specific sound type
    void PlayNotificationSound(SoundType type);

    // Play custom sound file
    void PlaySoundFile(const std::string& filepath);

    // Test sound with volume
    void TestSound(SoundType type, float volume = 0.5f);

    // Settings
    void SetCompletionSoundEnabled(bool enabled) { completion_sound_enabled_ = enabled; }
    void SetCompletionSoundType(SoundType type) { completion_sound_type_ = type; }
    void SetNotificationVolume(float volume) { notification_volume_ = volume; }
    void SetCustomSoundPath(const std::string& path) { custom_sound_path_ = path; }

    bool IsCompletionSoundEnabled() const { return completion_sound_enabled_; }
    SoundType GetCompletionSoundType() const { return completion_sound_type_; }
    float GetNotificationVolume() const { return notification_volume_; }
    std::string GetCustomSoundPath() const { return custom_sound_path_; }

    // Get available sounds
    std::vector<std::pair<SoundType, std::string>> GetAvailableSounds() const;
    std::string GetSoundName(SoundType type) const;

    // Log callback for debugging
    void SetLogCallback(std::function<void(const std::string&)> callback) {
        log_callback_ = callback;
    }

private:
    NotificationSounds() = default;
    ~NotificationSounds() = default;
    NotificationSounds(const NotificationSounds&) = delete;
    NotificationSounds& operator=(const NotificationSounds&) = delete;

    // Settings
    bool completion_sound_enabled_ = true;
    SoundType completion_sound_type_ = SoundType::WindowsDing;
    float notification_volume_ = 0.8f;  // Default volume at 80%
    std::string custom_sound_path_;

    // System
    bool initialized_ = false;
    std::function<void(const std::string&)> log_callback_;

    // Internal methods
    void PlayWindowsSystemSound(const std::string& sound_name);
    void PlayDistinctBeep(int frequency, int duration);
    void PlayDistinctBeepForType(SoundType type);
    void LogMessage(const std::string& message);
};

} // namespace SaperaCapturePro