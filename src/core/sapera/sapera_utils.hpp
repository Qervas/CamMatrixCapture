#pragma once

#include <SapClassBasic.h>
#include <functional>
#include <iostream>
#include <type_traits>
#include <memory>
#include <string>
#include <QDebug>

namespace sapera_utils {

// SFINAE detector for Create method
template<typename T, typename = void>
struct has_create : std::false_type {};

template<typename T>
struct has_create<T, std::void_t<decltype(std::declval<T>().Create())>> : std::true_type {};

// SFINAE detector for Destroy method
template<typename T, typename = void>
struct has_destroy : std::false_type {};

template<typename T>
struct has_destroy<T, std::void_t<decltype(std::declval<T>().Destroy())>> : std::true_type {};

// Safe Create wrapper with SFINAE
template<typename T>
typename std::enable_if<has_create<T>::value, bool>::type
safe_create(T* obj, const std::string& objName = "Object") {
    if (!obj) {
        qWarning() << "Null pointer to" << objName.c_str();
        return false;
    }
    
    try {
        // We can't use EnableConsoleOutput as it doesn't exist in this SDK version
        // Instead just use SetDisplayStatusMode
        SapManager::SetDisplayStatusMode(FALSE);
        bool result = obj->Create();
        
        if (!result) {
            qWarning() << "Failed to create" << objName.c_str();
        }
        return result;
    }
    catch (...) {
        qWarning() << "Exception during creation of" << objName.c_str();
        return false;
    }
}

// Safe Destroy wrapper with SFINAE
template<typename T>
typename std::enable_if<has_destroy<T>::value, void>::type
safe_destroy(T* obj, const std::string& objName = "Object") {
    if (!obj) return;
    
    try {
        // Suppress error dialogs during destroy
        SapManager::SetDisplayStatusMode(FALSE);
        obj->Destroy();
    }
    catch (...) {
        qWarning() << "Exception during destruction of" << objName.c_str();
    }
}

// Fallback for types without Destroy method
template<typename T>
typename std::enable_if<!has_destroy<T>::value, void>::type
safe_destroy(T*, const std::string& = "Object") {
    // Do nothing for types without destroy
}

// Template function to safely execute any Sapera operation
template<typename Func>
bool safe_sapera_op(Func&& func, const std::string& opName = "Sapera operation") {
    try {
        // Disable error dialogs during operation
        SapManager::SetDisplayStatusMode(FALSE);
        bool result = func();
        
        if (!result) {
            qWarning() << "Failed to perform" << opName.c_str();
        }
        return result;
    }
    catch (...) {
        qWarning() << "Exception during" << opName.c_str();
        return false;
    }
}

// Initialize Sapera SDK with error suppression
inline bool initialize_sapera() {
    // Disable all error dialogs
    SapManager::SetDisplayStatusMode(FALSE);
    return true;
}

} // namespace sapera_utils
