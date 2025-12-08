cmake_minimum_required(VERSION 3.16)

project(CameraMatrixCapture)

# Use C++20 for std::format and std::expected support
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows-specific settings
if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# =============================================================================
# SAPERA SDK CONFIGURATION
# =============================================================================

# Find Sapera SDK
set(SAPERA_ROOT "C:/Program Files/Teledyne DALSA/Sapera")
set(SAPERA_INCLUDE "${SAPERA_ROOT}/Include")
set(SAPERA_LIB "${SAPERA_ROOT}/Lib/Win64")

# Validate SDK
if(NOT EXISTS "${SAPERA_INCLUDE}")
    message(FATAL_ERROR "❌ Sapera SDK not found at: ${SAPERA_INCLUDE}")
endif()

message(STATUS "✅ Sapera SDK found: ${SAPERA_ROOT}")

# =============================================================================
# COMPILER CONFIGURATION
# =============================================================================

# MSVC specific settings
if(MSVC)
    add_compile_definitions(
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
    )

    add_compile_options(
        /W3                 # Warning level 3
        /MP                 # Multi-processor compilation
        /EHsc               # Enable exception handling
        /await              # Enable coroutines for WinRT
    )
    
endif()

# GCC specific settings for Windows SDK compatibility
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-fpermissive)
endif()

# =============================================================================
# BACKEND DLL (for WinUI frontend)
# =============================================================================

# Core backend library as DLL
add_library(CaptureCore SHARED
    src/api/CaptureAPI.cpp
    src/api/CaptureStateMachine.cpp
    src/hardware/CameraTypes.cpp
    src/hardware/CameraManager.cpp
    src/utils/SessionManager.cpp
    src/utils/SettingsManager.cpp
    src/utils/NotificationSounds.cpp
    src/bluetooth/BluetoothManager.cpp
    src/bluetooth/BluetoothDevice.cpp
    src/bluetooth/BluetoothScanner.cpp
    src/bluetooth/TurntableController.cpp
)

target_compile_definitions(CaptureCore PRIVATE CAPTURECORE_EXPORTS)

target_include_directories(CaptureCore PRIVATE
    src
    "C:/Program Files/Teledyne DALSA/Sapera/Classes/Basic"
    "C:/Program Files/Teledyne DALSA/Sapera/Include"
)

target_link_directories(CaptureCore PRIVATE
    "C:/Program Files/Teledyne DALSA/Sapera/Lib/Win64"
)

target_link_libraries(CaptureCore PRIVATE
    SapClassBasic.lib
    windowsapp.lib  # For WinRT/C++
)

# WinRT specific settings
if(MSVC)
    set_property(TARGET CaptureCore PROPERTY VS_GLOBAL_ConsumeWinRTExtension true)
    set_property(TARGET CaptureCore PROPERTY VS_GLOBAL_AdditionalUsingDirectories "$(WindowsSDK_WindowsMetadata)")
endif()

# =============================================================================
# INSTALLATION
# =============================================================================

install(TARGETS CaptureCore
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)

# Copy DLL to WinUI App output directory
add_custom_command(TARGET CaptureCore POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:CaptureCore>
        ${CMAKE_SOURCE_DIR}/App/bin/x64/$<CONFIG>/net8.0-windows10.0.19041.0/
) 