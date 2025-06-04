cmake_minimum_required(VERSION 3.15)

project(CleanSaperaSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sapera SDK Paths
set(SAPERA_SDK_ROOT "C:/Program Files/Teledyne DALSA/Sapera")
set(SAPERA_SDK_INCLUDE "${SAPERA_SDK_ROOT}/Include")
set(SAPERA_SDK_CLASSES_BASIC "${SAPERA_SDK_ROOT}/Classes/Basic")
set(SAPERA_SDK_LIB "${SAPERA_SDK_ROOT}/Lib/Win64")

# Check if Sapera SDK exists
if(NOT EXISTS "${SAPERA_SDK_INCLUDE}")
    message(FATAL_ERROR "Sapera SDK not found at: ${SAPERA_SDK_INCLUDE}")
endif()

# Include directories
include_directories(${SAPERA_SDK_INCLUDE})
include_directories(${SAPERA_SDK_CLASSES_BASIC})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/backend/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/backend/src/lib)

# ==============================================
# CLEAN SAPERA CAMERA SYSTEM - WORKING VERSION
# ==============================================

# Main camera capture system with improved dark image handling
set(CAPTURE_SOURCES
    backend/src/refactored_capture.cpp
    backend/src/CameraConfigManager.cpp
    backend/src/CameraAPI.cpp
)

# Build the camera capture executable
add_executable(refactored_capture ${CAPTURE_SOURCES})

# Link Sapera SDK libraries
target_link_libraries(refactored_capture
    ${SAPERA_SDK_LIB}/SapClassBasic.lib
    ${SAPERA_SDK_LIB}/corapi.lib
    user32.lib
    gdi32.lib
    advapi32.lib
    ole32.lib
    oleaut32.lib
    uuid.lib
    comdlg32.lib
    kernel32.lib
    winmm.lib
    ws2_32.lib
)

# Set output directory
set_target_properties(refactored_capture PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Add compiler warnings
target_compile_options(refactored_capture PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W3>
)

# ==============================================
# POST-BUILD: ORGANIZE SYSTEM STRUCTURE
# ==============================================

add_custom_command(TARGET refactored_capture POST_BUILD
    COMMENT "🚀 Organizing Clean Sapera System..."
    
    # Copy executable to backend directory
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:refactored_capture>"
        "${CMAKE_CURRENT_SOURCE_DIR}/backend/refactored_capture.exe"
        
    # Copy configuration
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/camera_config.json"
        "${CMAKE_BINARY_DIR}/camera_config.json"
        
    COMMAND ${CMAKE_COMMAND} -E echo "✅ Clean Sapera System ready!"
    COMMAND ${CMAKE_COMMAND} -E echo "📁 Backend: backend/refactored_capture.exe"
    COMMAND ${CMAKE_COMMAND} -E echo "🌐 Web: python web-bridge/web_bridge.py"
    COMMAND ${CMAKE_COMMAND} -E echo "🖥️  Frontend: frontend/index.html"
)

# ==============================================
# BUILD INFORMATION
# ==============================================

message(STATUS "==============================================")
message(STATUS "Clean Sapera Camera System - Simplified Build")
message(STATUS "==============================================")
message(STATUS "Sapera SDK: ${SAPERA_SDK_ROOT}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "")
message(STATUS "🎯 Building ONLY working components:")
message(STATUS "   📸 refactored_capture.exe - Camera system with improved retry")
message(STATUS "   🌐 web_bridge.py - FastAPI server")
message(STATUS "   🖥️  index.html - Frontend dashboard")
message(STATUS "")
message(STATUS "🔧 Features:")
message(STATUS "   • Intelligent dark image retry (2x exposure, 1.5x gain)")
message(STATUS "   • Longer settling times (300-500ms)")
message(STATUS "   • Quality analysis and auto-retry")
message(STATUS "   • Clean modular structure")
message(STATUS "==============================================") 