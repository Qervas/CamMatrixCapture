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

# Set Qt6 path
set(CMAKE_PREFIX_PATH "C:/Qt/6.8.0/msvc2022_64" ${CMAKE_PREFIX_PATH})

# Find required packages
find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

# Find Sapera SDK
set(SAPERA_SDK_PATH "C:/Program Files/Teledyne/Sapera")
set(GIGE_VISION_PATH "C:/Program Files/Teledyne/GigE Vision Interface")

# Since we're using our own mock implementation in sapera_defs.hpp,
# we don't need to check for the actual SDK headers anymore.
# Just set SAPERA_FOUND to true so our code compiles with mocks
set(SAPERA_FOUND TRUE)
message(STATUS "Using mock Sapera implementation defined in sapera_defs.hpp")

# Add preprocessor definitions
add_definitions(-DSAPERA_FOUND)
add_definitions(-DHAS_SAPERA=1)
add_definitions(-DHAS_GIGE_VISION=0)

# For informational purposes only
if(EXISTS "${SAPERA_SDK_PATH}")
    message(STATUS "Found Teledyne Sapera SDK at: ${SAPERA_SDK_PATH}")
    set(SAPERA_DIR "${SAPERA_SDK_PATH}")
elseif(EXISTS "${GIGE_VISION_PATH}")
    message(STATUS "Found Teledyne GigE Vision Interface at: ${GIGE_VISION_PATH}")
    message(STATUS "Note: Using our own mock Sapera implementation")
    set(SAPERA_DIR "${GIGE_VISION_PATH}")
else()
    message(WARNING "Neither Sapera SDK nor GigE Vision Interface found.")
    message(STATUS "Using mock implementation only.")
endif()

# Add include directories if either SDK exists (optional)
if(EXISTS "${SAPERA_DIR}")
    include_directories(${SAPERA_DIR}/Classes/Basic)
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
