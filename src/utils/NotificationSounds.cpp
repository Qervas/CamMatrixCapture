#include "NotificationSounds.hpp"
#define NOMINMAX
#include <Windows.h>

namespace SaperaCapturePro {

NotificationSounds& NotificationSounds::Instance() {
    static NotificationSounds instance;
    return instance;
}

void NotificationSounds::Initialize() {
    if (initialized_) return;

    LogMessage("[SOUND] Notification sound system initialized (using Windows default alert)");
    initialized_ = true;
}

void NotificationSounds::PlayCompletionSound() {
    if (!completion_sound_enabled_) {
        LogMessage("[SOUND] Completion sound disabled");
        return;
    }

    if (!initialized_) {
        Initialize();
    }

    // Play Windows default alert sound using MessageBeep
    // MB_ICONASTERISK plays the system's default notification sound
    LogMessage("[SOUND] Playing Windows default alert sound");
    MessageBeep(MB_ICONASTERISK);
}

void NotificationSounds::LogMessage(const std::string& message) {
    if (log_callback_) {
        log_callback_(message);
    }
}

} // namespace SaperaCapturePro