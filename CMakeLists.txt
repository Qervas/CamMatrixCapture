cmake_minimum_required(VERSION 3.20)

project(NeuralRenderingCapture)

# Use C++20 for std::format and std::expected support
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows-specific settings
if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Find OpenGL
find_package(OpenGL REQUIRED)

# GLFW - Download and build
include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.9
    GIT_SHALLOW TRUE
    GIT_SUBMODULES ""
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Dear ImGui - Download and build (DOCKING BRANCH)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
    GIT_SHALLOW TRUE
    GIT_SUBMODULES ""
)
FetchContent_MakeAvailable(imgui)

# Create ImGui library
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC 
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

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
# MAIN EXECUTABLE
# =============================================================================

# Main GUI application
add_executable(neural_capture
    src/main.cpp  # Now using the modular architecture
    src/hardware/CameraTypes.cpp
    src/hardware/CameraManager.cpp
    src/utils/SessionManager.cpp
    src/utils/SettingsManager.cpp
    src/bluetooth/BluetoothManager.cpp
    src/bluetooth/BluetoothDevice.cpp
    src/bluetooth/BluetoothScanner.cpp
    src/gui/BluetoothGui.cpp
    src/gui/GuiManager.cpp
    src/gui/PreferencesDialog.cpp
    src/gui/LogPanel.cpp
    src/gui/Application.cpp
)

target_include_directories(neural_capture PRIVATE
    src
    "C:/Program Files/Teledyne DALSA/Sapera/Classes/Basic"
    "C:/Program Files/Teledyne DALSA/Sapera/Include"
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_directories(neural_capture PRIVATE
    "C:/Program Files/Teledyne DALSA/Sapera/Lib/Win64"
)

target_link_libraries(neural_capture PRIVATE
    imgui
    SapClassBasic.lib
    windowsapp.lib  # For WinRT/C++
)

# WinRT specific settings (based on working BLE demo)
if(MSVC)
    set_property(TARGET neural_capture PROPERTY VS_GLOBAL_ConsumeWinRTExtension true)
    set_property(TARGET neural_capture PROPERTY VS_GLOBAL_AdditionalUsingDirectories "$(WindowsSDK_WindowsMetadata)")
endif()

# Set as default target
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT neural_capture)

# =============================================================================
# INSTALLATION
# =============================================================================

install(TARGETS neural_capture
    RUNTIME DESTINATION bin
) 