cmake_minimum_required(VERSION 3.16)

# Project name and version
project(CamMatrixCapture VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find required packages
find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

# Check for Sapera SDK
if(WIN32)
    # Look for Sapera SDK in the default installation path
    if(EXISTS "C:/Program Files/Teledyne/Sapera")
        set(SAPERA_DIR "C:/Program Files/Teledyne/Sapera" CACHE PATH "Path to Sapera SDK")
        set(SAPERA_FOUND TRUE)
    else()
        set(SAPERA_FOUND FALSE)
        message(WARNING "Sapera SDK not found. Please install it or set SAPERA_DIR manually.")
    endif()
endif()

# Add source files
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/*.hpp"
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Add Sapera SDK if found
if(SAPERA_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${SAPERA_DIR}/Classes/Basic
    )
    target_link_directories(${PROJECT_NAME} PRIVATE
        ${SAPERA_DIR}/Lib
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        SapClassBasic
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        SAPERA_FOUND
    )
endif()

# Automatically deploy Qt DLLs on Windows
if(WIN32)
    # Get the Qt binary directory
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BINARY_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)

    # Use windeployqt in post-build step
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${QT_BINARY_DIR}/windeployqt.exe"
            --verbose 1
            --no-compiler-runtime
            --no-translations
            --no-system-d3d-compiler
            --debug
            --plugindir "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins"
            "$<TARGET_FILE:${PROJECT_NAME}>"
        COMMENT "Deploying Qt dependencies..."
    )
endif()

# Install targets
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Print configuration summary
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Output Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "Qt Version: ${Qt6_VERSION}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Source files found: ${SOURCES}")
