/**
 * Professional File System Utilities
 * Cross-platform file system operations for enterprise applications
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <cstdint>
#include <thread>
#include <atomic>

namespace SaperaCapturePro {

/**
 * Professional file system utilities
 */
class FileSystem {
public:
    // Path operations
    static bool exists(const std::string& path);
    static bool isFile(const std::string& path);
    static bool isDirectory(const std::string& path);
    static std::string getAbsolutePath(const std::string& path);
    static std::string getParentDirectory(const std::string& path);
    static std::string getFileName(const std::string& path);
    static std::string getFileExtension(const std::string& path);
    static std::string getFileNameWithoutExtension(const std::string& path);
    
    // Directory operations
    static bool createDirectory(const std::string& path);
    static bool createDirectories(const std::string& path);
    static bool removeDirectory(const std::string& path, bool recursive = false);
    static std::vector<std::string> listDirectory(const std::string& path);
    static std::vector<std::string> listFiles(const std::string& path, 
                                             const std::string& pattern = "*");
    
    // File operations
    static bool copyFile(const std::string& source, const std::string& destination);
    static bool moveFile(const std::string& source, const std::string& destination);
    static bool removeFile(const std::string& path);
    static size_t getFileSize(const std::string& path);
    static std::chrono::system_clock::time_point getLastModified(const std::string& path);
    
    // Content operations
    static std::string readTextFile(const std::string& path);
    static bool writeTextFile(const std::string& path, const std::string& content);
    static std::vector<uint8_t> readBinaryFile(const std::string& path);
    static bool writeBinaryFile(const std::string& path, const std::vector<uint8_t>& data);
    
    // Path utilities
    static std::string joinPath(const std::string& path1, const std::string& path2);
    static std::string joinPath(const std::vector<std::string>& parts);
    static std::string normalizePath(const std::string& path);
    static std::string getCurrentDirectory();
    static bool setCurrentDirectory(const std::string& path);
    
    // Temporary files
    static std::string getTempDirectory();
    static std::string createTempFile(const std::string& prefix = "tmp", 
                                     const std::string& extension = ".tmp");
    static std::string createTempDirectory(const std::string& prefix = "tmp");
    
    // File watching (for configuration reload)
    class FileWatcher;
    static std::unique_ptr<FileWatcher> createFileWatcher(const std::string& path);
    
    // Disk space
    static uint64_t getAvailableSpace(const std::string& path);
    static uint64_t getTotalSpace(const std::string& path);
    
    // Platform-specific
    static char getPathSeparator();
    static std::string getExecutablePath();
    static std::string getExecutableDirectory();
    
private:
    FileSystem() = delete; // Static class only
};

/**
 * File watcher for configuration changes
 */
class FileSystem::FileWatcher {
public:
    using ChangeCallback = std::function<void(const std::string& path)>;
    
    virtual ~FileWatcher() = default;
    virtual bool start(const ChangeCallback& callback) = 0;
    virtual void stop() = 0;
    virtual bool isWatching() const = 0;
};

} // namespace SaperaCapturePro 