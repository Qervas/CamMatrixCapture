#pragma once

#include <string>
#include <functional>

namespace SaperaCapturePro {

class NotificationSounds {
public:
    static NotificationSounds& Instance();

    // Initialize sound system
    void Initialize();

    // Play completion sound with current settings
    void PlayCompletionSound();

    // Settings
    void SetCompletionSoundEnabled(bool enabled) { completion_sound_enabled_ = enabled; }
    bool IsCompletionSoundEnabled() const { return completion_sound_enabled_; }

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

    // System
    bool initialized_ = false;
    std::function<void(const std::string&)> log_callback_;

    // Internal method
    void LogMessage(const std::string& message);
};

} // namespace SaperaCapturePro