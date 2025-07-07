cmake_minimum_required(VERSION 3.20)

project(NeuralRenderingCapture)

# Use C++17 for better compatibility
set(CMAKE_CXX_STANDARD 17)
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
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Dear ImGui - Download and build
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.1
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
# EXECUTABLES
# =============================================================================

# Console version (original working)
add_executable(neural_capture_console
    src/main.cpp
    src/hardware/CameraTypes.cpp
)

target_include_directories(neural_capture_console PRIVATE
    src
    "C:/Program Files/Teledyne DALSA/Sapera/Classes/Basic"
    "C:/Program Files/Teledyne DALSA/Sapera/Include"
)

target_link_directories(neural_capture_console PRIVATE
    "C:/Program Files/Teledyne DALSA/Sapera/Lib/Win64"
)

target_link_libraries(neural_capture_console PRIVATE
    SapClassBasic
)

# INTEGRATED GUI version (REAL Sapera SDK integration)
add_executable(neural_capture_integrated
    src/main_gui_integrated.cpp
    src/gui/NeuralCaptureGUI.cpp
    src/capture/NeuralCaptureSystem.cpp
    src/capture/ParameterController.cpp
    src/hardware/CameraTypes.cpp
)

target_include_directories(neural_capture_integrated PRIVATE
    src
    "C:/Program Files/Teledyne DALSA/Sapera/Classes/Basic"
    "C:/Program Files/Teledyne DALSA/Sapera/Include"
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_directories(neural_capture_integrated PRIVATE
    "C:/Program Files/Teledyne DALSA/Sapera/Lib/Win64"
)

target_link_libraries(neural_capture_integrated PRIVATE
    imgui
    SapClassBasic
)

# Set the INTEGRATED version as the default target
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT neural_capture_integrated)

# DIRECT GUI version (Simple, everything in one file)
add_executable(neural_capture_direct
    src/main_gui_direct.cpp
    src/hardware/CameraTypes.cpp
)

target_include_directories(neural_capture_direct PRIVATE
    src
    "C:/Program Files/Teledyne DALSA/Sapera/Classes/Basic"
    "C:/Program Files/Teledyne DALSA/Sapera/Include"
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_directories(neural_capture_direct PRIVATE
    "C:/Program Files/Teledyne DALSA/Sapera/Lib/Win64"
)

target_link_libraries(neural_capture_direct PRIVATE
    imgui
    SapClassBasic
)

# Set the DIRECT version as the default target
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT neural_capture_direct)

# =============================================================================
# INSTALLATION
# =============================================================================

install(TARGETS neural_capture_console neural_capture_integrated
    RUNTIME DESTINATION bin
) 