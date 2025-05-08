#pragma once

#include <QString>
#include <QVariant>
#include <QSettings>
#include <QDir>

namespace cam_matrix::core {

/**
 * Application settings management
 */
class Settings {
public:
    /**
     * Get a settings value
     * @param key The settings key
     * @param defaultValue The default value to return if key is not found
     * @return The settings value or default value
     */
    static QVariant getValue(const QString& key, const QVariant& defaultValue = QVariant()) {
        QSettings settings;
        return settings.value(key, defaultValue);
    }
    
    /**
     * Set a settings value
     * @param key The settings key
     * @param value The value to set
     */
    static void setValue(const QString& key, const QVariant& value) {
        QSettings settings;
        settings.setValue(key, value);
    }
    
    /**
     * Get the directory for saving captured photos
     * @return The directory path
     */
    static QString getPhotoSaveDirectory() {
        return getValue("camera/savePath", QDir::homePath()).toString();
    }
    
    /**
     * Set the directory for saving captured photos
     * @param path The directory path
     */
    static void setPhotoSaveDirectory(const QString& path) {
        setValue("camera/savePath", path);
    }
    
    /**
     * Get the application theme
     * @return The theme name ("light", "dark", or "system")
     */
    static QString getTheme() {
        return getValue("app/theme", "system").toString();
    }
    
    /**
     * Set the application theme
     * @param theme The theme name ("light", "dark", or "system")
     */
    static void setTheme(const QString& theme) {
        setValue("app/theme", theme);
    }
};

} // namespace cam_matrix::core 