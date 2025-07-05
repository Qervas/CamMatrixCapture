/**
 * Professional File System Utilities Implementation
 * Cross-platform file system operations for enterprise applications
 */

#include "FileSystem.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <direct.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#endif

namespace SaperaCapturePro {

// Path operations
bool FileSystem::exists(const std::string& path) {
    try {
        return std::filesystem::exists(path);
    } catch (...) {
        return false;
    }
}

bool FileSystem::isFile(const std::string& path) {
    try {
        return std::filesystem::is_regular_file(path);
    } catch (...) {
        return false;
    }
}

bool FileSystem::isDirectory(const std::string& path) {
    try {
        return std::filesystem::is_directory(path);
    } catch (...) {
        return false;
    }
}

std::string FileSystem::getAbsolutePath(const std::string& path) {
    try {
        return std::filesystem::absolute(path).string();
    } catch (...) {
        return path; // Return original path if conversion fails
    }
}

std::string FileSystem::getParentDirectory(const std::string& path) {
    try {
        return std::filesystem::path(path).parent_path().string();
    } catch (...) {
        return "";
    }
}

std::string FileSystem::getFileName(const std::string& path) {
    try {
        return std::filesystem::path(path).filename().string();
    } catch (...) {
        return "";
    }
}

std::string FileSystem::getFileExtension(const std::string& path) {
    try {
        return std::filesystem::path(path).extension().string();
    } catch (...) {
        return "";
    }
}

std::string FileSystem::getFileNameWithoutExtension(const std::string& path) {
    try {
        return std::filesystem::path(path).stem().string();
    } catch (...) {
        return "";
    }
}

// Directory operations
bool FileSystem::createDirectory(const std::string& path) {
    try {
        return std::filesystem::create_directory(path);
    } catch (...) {
        return false;
    }
}

bool FileSystem::createDirectories(const std::string& path) {
    try {
        return std::filesystem::create_directories(path);
    } catch (...) {
        return false;
    }
}

bool FileSystem::removeDirectory(const std::string& path, bool recursive) {
    try {
        if (recursive) {
            return std::filesystem::remove_all(path) > 0;
        } else {
            return std::filesystem::remove(path);
        }
    } catch (...) {
        return false;
    }
}

std::vector<std::string> FileSystem::listDirectory(const std::string& path) {
    std::vector<std::string> result;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            result.push_back(entry.path().filename().string());
        }
    } catch (...) {
        // Return empty vector on error
    }
    
    return result;
}

std::vector<std::string> FileSystem::listFiles(const std::string& path, const std::string& pattern) {
    std::vector<std::string> result;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                
                // Simple pattern matching (just "*" for now)
                if (pattern == "*" || filename.find(pattern) != std::string::npos) {
                    result.push_back(filename);
                }
            }
        }
    } catch (...) {
        // Return empty vector on error
    }
    
    return result;
}

// File operations
bool FileSystem::copyFile(const std::string& source, const std::string& destination) {
    try {
        return std::filesystem::copy_file(source, destination, 
                                         std::filesystem::copy_options::overwrite_existing);
    } catch (...) {
        return false;
    }
}

bool FileSystem::moveFile(const std::string& source, const std::string& destination) {
    try {
        std::filesystem::rename(source, destination);
        return true;
    } catch (...) {
        return false;
    }
}

bool FileSystem::removeFile(const std::string& path) {
    try {
        return std::filesystem::remove(path);
    } catch (...) {
        return false;
    }
}

size_t FileSystem::getFileSize(const std::string& path) {
    try {
        return static_cast<size_t>(std::filesystem::file_size(path));
    } catch (...) {
        return 0;
    }
}

std::chrono::system_clock::time_point FileSystem::getLastModified(const std::string& path) {
    try {
        auto ftime = std::filesystem::last_write_time(path);
        // Convert filesystem time to system_clock time
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        return sctp;
    } catch (...) {
        return std::chrono::system_clock::now();
    }
}

// Content operations
std::string FileSystem::readTextFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    return content;
}

bool FileSystem::writeTextFile(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    file << content;
    file.close();
    return file.good();
}

std::vector<uint8_t> FileSystem::readBinaryFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    file.close();
    
    return buffer;
}

bool FileSystem::writeBinaryFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    file.close();
    return file.good();
}

// Path utilities
std::string FileSystem::joinPath(const std::string& path1, const std::string& path2) {
    try {
        std::filesystem::path p1(path1);
        std::filesystem::path p2(path2);
        return (p1 / p2).string();
    } catch (...) {
        return path1 + getPathSeparator() + path2;
    }
}

std::string FileSystem::joinPath(const std::vector<std::string>& parts) {
    if (parts.empty()) {
        return "";
    }
    
    std::filesystem::path result(parts[0]);
    for (size_t i = 1; i < parts.size(); ++i) {
        result /= parts[i];
    }
    
    return result.string();
}

std::string FileSystem::normalizePath(const std::string& path) {
    try {
        return std::filesystem::path(path).lexically_normal().string();
    } catch (...) {
        return path;
    }
}

std::string FileSystem::getCurrentDirectory() {
    try {
        return std::filesystem::current_path().string();
    } catch (...) {
        return "";
    }
}

bool FileSystem::setCurrentDirectory(const std::string& path) {
    try {
        std::filesystem::current_path(path);
        return true;
    } catch (...) {
        return false;
    }
}

// Temporary files
std::string FileSystem::getTempDirectory() {
    try {
        return std::filesystem::temp_directory_path().string();
    } catch (...) {
#ifdef _WIN32
        return "C:\\Temp";
#else
        return "/tmp";
#endif
    }
}

std::string FileSystem::createTempFile(const std::string& prefix, const std::string& extension) {
    std::string tempDir = getTempDirectory();
    
    // Generate a unique filename using timestamp and random suffix
    auto now = std::chrono::high_resolution_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::string filename = prefix + "_" + std::to_string(timestamp) + extension;
    return joinPath(tempDir, filename);
}

std::string FileSystem::createTempDirectory(const std::string& prefix) {
    std::string tempDir = getTempDirectory();
    
    // Generate a unique directory name
    auto now = std::chrono::high_resolution_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::string dirname = prefix + "_" + std::to_string(timestamp);
    std::string fullPath = joinPath(tempDir, dirname);
    
    if (createDirectories(fullPath)) {
        return fullPath;
    }
    
    return "";
}

// File watching (basic implementation - could be enhanced with platform-specific APIs)
class BasicFileWatcher : public FileSystem::FileWatcher {
public:
    BasicFileWatcher(const std::string& path) : path_(path), watching_(false) {}
    
    bool start(const ChangeCallback& callback) override {
        if (watching_) {
            return false;
        }
        
        callback_ = callback;
        watching_ = true;
        
        // Start monitoring thread (simplified implementation)
        watchThread_ = std::thread([this]() {
            auto lastModified = FileSystem::getLastModified(path_);
            
            while (watching_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Check every second
                
                if (!watching_) break;
                
                auto currentModified = FileSystem::getLastModified(path_);
                if (currentModified != lastModified) {
                    lastModified = currentModified;
                    if (callback_) {
                        callback_(path_);
                    }
                }
            }
        });
        
        return true;
    }
    
    void stop() override {
        watching_ = false;
        if (watchThread_.joinable()) {
            watchThread_.join();
        }
    }
    
    bool isWatching() const override {
        return watching_;
    }
    
private:
    std::string path_;
    std::atomic<bool> watching_;
    ChangeCallback callback_;
    std::thread watchThread_;
};

std::unique_ptr<FileSystem::FileWatcher> FileSystem::createFileWatcher(const std::string& path) {
    return std::make_unique<BasicFileWatcher>(path);
}

// Disk space
uint64_t FileSystem::getAvailableSpace(const std::string& path) {
    try {
        auto spaceInfo = std::filesystem::space(path);
        return static_cast<uint64_t>(spaceInfo.available);
    } catch (...) {
        return 0;
    }
}

uint64_t FileSystem::getTotalSpace(const std::string& path) {
    try {
        auto spaceInfo = std::filesystem::space(path);
        return static_cast<uint64_t>(spaceInfo.capacity);
    } catch (...) {
        return 0;
    }
}

// Platform-specific
char FileSystem::getPathSeparator() {
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

std::string FileSystem::getExecutablePath() {
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return std::string(path);
#else
    char path[1024];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        return std::string(path);
    }
    return "";
#endif
}

std::string FileSystem::getExecutableDirectory() {
    std::string exePath = getExecutablePath();
    return getParentDirectory(exePath);
}

} // namespace SaperaCapturePro 