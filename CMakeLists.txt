cmake_minimum_required(VERSION 3.20)

project(NeuralRenderingCapture)

# Use C++17 for better compatibility
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(SAPERA REQUIRED)

# Dear ImGui and GLFW for GUI
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

# Dear ImGui - using FetchContent for latest version
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.0
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

# Find Sapera SDK - Updated correct paths
set(SAPERA_ROOT "C:/Program Files/Teledyne DALSA/Sapera")
set(SAPERA_INCLUDE "${SAPERA_ROOT}/Include")
set(SAPERA_LIB "${SAPERA_ROOT}/Lib/Win64")

# Validate SDK
if(NOT EXISTS "${SAPERA_INCLUDE}")
    message(FATAL_ERROR "❌ Sapera SDK not found at: ${SAPERA_INCLUDE}")
endif()

message(STATUS "✅ Sapera SDK found: ${SAPERA_ROOT}")
message(STATUS "✅ Include: ${SAPERA_INCLUDE}")
message(STATUS "✅ Lib: ${SAPERA_LIB}")

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
    )
endif()

# =============================================================================
# EXECUTABLES - Keep it simple and working
# =============================================================================

# Main executable
add_executable(neural_capture_gui
    src/main.cpp
    src/hardware/CameraTypes.cpp
    src/gui/NeuralCaptureGUI.cpp
    src/gui/CameraControlPanel.cpp
    src/gui/ParameterPanel.cpp
    src/gui/StatusPanel.cpp
    src/gui/CapturePanel.cpp
)

target_include_directories(neural_capture_gui PRIVATE
    src
    "C:/Program Files/Teledyne DALSA/Sapera/Classes/Basic"
    "C:/Program Files/Teledyne DALSA/Sapera/Include"
)

target_link_directories(neural_capture_gui PRIVATE
    "C:/Program Files/Teledyne DALSA/Sapera/Lib/Win64"
)

target_link_libraries(neural_capture_gui PRIVATE
    imgui
    glfw
    OpenGL::GL
    SapClassBasic
)

# Copy required DLLs to output directory
add_custom_command(TARGET neural_capture_gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "C:/Program Files/Teledyne DALSA/Sapera/Bin/Win64/SapClassBasic.dll"
    $<TARGET_FILE_DIR:neural_capture_gui>
)

# =============================================================================
# INSTALLATION
# =============================================================================

install(TARGETS neural_capture_gui
    RUNTIME DESTINATION bin
) 