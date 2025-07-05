cmake_minimum_required(VERSION 3.16)

project(SaperaCapturePro 
    VERSION 1.0.0
    LANGUAGES CXX
)

# Use C++17 for better compatibility
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# Main working application
add_executable(sapera_capture_pro
    src/main.cpp
    src/hardware/CameraTypes.hpp
    src/hardware/CameraTypes.cpp
)

target_include_directories(sapera_capture_pro
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${SAPERA_INCLUDE}
        ${SAPERA_ROOT}/Classes/Basic
)

target_link_libraries(sapera_capture_pro
    PRIVATE
        "${SAPERA_LIB}/SapClassBasic.lib"
        "${SAPERA_LIB}/corapi.lib"
)

# =============================================================================
# INSTALLATION
# =============================================================================

install(TARGETS sapera_capture_pro
    RUNTIME DESTINATION bin
) 