cmake_minimum_required(VERSION 3.15)

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

# Add /FS flag for Visual Studio to fix PDB file writing conflicts
if(MSVC)
    add_compile_options(/FS)
endif()

# Set Qt6 path
set(CMAKE_PREFIX_PATH "C:/Qt/6.8.0/msvc2022_64" ${CMAKE_PREFIX_PATH})

# Find required packages
find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

# Find Sapera SDK - using exact paths
set(SAPERA_SDK_PATH "C:/Program Files/Teledyne DALSA/Sapera")
set(SAPERA_INCLUDE_DIR "${SAPERA_SDK_PATH}/Include")
set(SAPERA_CLASSES_DIR "${SAPERA_SDK_PATH}/Classes")
set(SAPERA_CLASSES_BASIC_DIR "${SAPERA_SDK_PATH}/Classes/Basic")
set(SAPERA_CLASSES_GUI_DIR "${SAPERA_SDK_PATH}/Classes/Gui")
set(SAPERA_LIB_DIR "${SAPERA_SDK_PATH}/Lib/Win64")
set(SAPERA_LIB_VS2019_DIR "${SAPERA_LIB_DIR}/VS2019")

# Define library paths based on user's system
set(CORAPI_LIB "${SAPERA_LIB_DIR}/corapi.lib")
set(SAPCLASSBASIC_LIB "${SAPERA_LIB_DIR}/SapClassBasic.lib")
set(SAPCLASSGUI_LIB "${SAPERA_LIB_VS2019_DIR}/SapClassGui.lib")

# Check if Sapera SDK is available
if(EXISTS "${SAPERA_CLASSES_BASIC_DIR}/SapClassBasic.h" AND EXISTS "${SAPCLASSBASIC_LIB}")
    message(STATUS "Found Teledyne Sapera SDK at: ${SAPERA_SDK_PATH}")
    
    # Check if we can use the full Sapera SDK or just the stub implementation
    option(USE_MOCK_SAPERA "Use mock Sapera implementation even if SDK is found" FALSE)
    
    if(USE_MOCK_SAPERA)
        # Use mock implementation
        set(SAPERA_FOUND FALSE)
        set(HAS_SAPERA 0)
        set(HAS_GIGE_VISION 1)
        add_definitions(-DSAPERA_FOUND=0)
        add_definitions(-DHAS_SAPERA=0)
        add_definitions(-DHAS_GIGE_VISION=1)
        message(STATUS "Using mock Sapera implementation (forced by USE_MOCK_SAPERA)")
    else()
        # Use real SDK
        set(SAPERA_FOUND TRUE)
        set(HAS_SAPERA 1)
        set(HAS_GIGE_VISION 0)
        add_definitions(-DSAPERA_FOUND=1)
        add_definitions(-DHAS_SAPERA=1)
        add_definitions(-DHAS_GIGE_VISION=0)
    endif()
    
    # Print detected directories for debugging
    message(STATUS "Using include directories:")
    message(STATUS "  - ${SAPERA_INCLUDE_DIR}")
    message(STATUS "  - ${SAPERA_CLASSES_DIR}")
    message(STATUS "  - ${SAPERA_CLASSES_BASIC_DIR}")
    message(STATUS "  - ${SAPERA_CLASSES_GUI_DIR}")
    message(STATUS "Using library directories:")
    message(STATUS "  - ${SAPERA_LIB_DIR}")
    message(STATUS "  - ${SAPERA_LIB_VS2019_DIR}")
    
    # Check required libraries exist
    if(NOT EXISTS "${CORAPI_LIB}")
        message(WARNING "corapi.lib not found at ${CORAPI_LIB}")
        set(CORAPI_LIB "")
    else()
        message(STATUS "Found corapi.lib: ${CORAPI_LIB}")
    endif()
    
    if(NOT EXISTS "${SAPCLASSBASIC_LIB}")
        message(WARNING "SapClassBasic.lib not found at ${SAPCLASSBASIC_LIB}")
        set(SAPCLASSBASIC_LIB "")
    else()
        message(STATUS "Found SapClassBasic.lib: ${SAPCLASSBASIC_LIB}")
    endif()
    
    if(NOT EXISTS "${SAPCLASSGUI_LIB}")
        message(WARNING "SapClassGui.lib not found at ${SAPCLASSGUI_LIB} - will not link")
        set(SAPCLASSGUI_LIB "")
    else()
        message(STATUS "Found SapClassGui.lib: ${SAPCLASSGUI_LIB}")
    endif()
    
    # Final check if we can continue with real SDK or need to fallback to mock
    if(NOT EXISTS "${CORAPI_LIB}" OR NOT EXISTS "${SAPCLASSBASIC_LIB}")
        message(WARNING "Required Sapera libraries not found - falling back to mock implementation")
        set(SAPERA_FOUND FALSE)
        set(HAS_SAPERA 0)
        set(HAS_GIGE_VISION 1)
        add_definitions(-DSAPERA_FOUND=0)
        add_definitions(-DHAS_SAPERA=0)
        add_definitions(-DHAS_GIGE_VISION=1)
    endif()
else()
    # Fallback to mock mode
    set(SAPERA_FOUND FALSE)
    set(HAS_SAPERA 0)
    set(HAS_GIGE_VISION 1)
    add_definitions(-DSAPERA_FOUND=0)
    add_definitions(-DHAS_SAPERA=0)
    add_definitions(-DHAS_GIGE_VISION=1)
    message(WARNING "Using mock Sapera implementation - Sapera SDK not found")
    message(STATUS "Checked for: ${SAPERA_CLASSES_BASIC_DIR}/SapClassBasic.h")
endif()

# Add debugging flag in debug builds for additional error output
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG_MODE=1)
endif()

# Add source files
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/*.hpp"
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Add Sapera include and library directories if found
if(SAPERA_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${SAPERA_INCLUDE_DIR}
        ${SAPERA_CLASSES_DIR}
        ${SAPERA_CLASSES_BASIC_DIR}
        ${SAPERA_CLASSES_GUI_DIR}
    )
    
    # Link directories
    link_directories(${SAPERA_LIB_DIR})
    
    # Link with Sapera libraries
    if(EXISTS "${CORAPI_LIB}")
        target_link_libraries(${PROJECT_NAME} PRIVATE "${CORAPI_LIB}")
    endif()
    
    if(EXISTS "${SAPCLASSBASIC_LIB}")
        target_link_libraries(${PROJECT_NAME} PRIVATE "${SAPCLASSBASIC_LIB}")
    endif()
    
    if(EXISTS "${SAPCLASSGUI_LIB}")
        target_link_libraries(${PROJECT_NAME} PRIVATE "${SAPCLASSGUI_LIB}")
    endif()
    
    # Copy Sapera DLLs to output directory
    file(GLOB SAPERA_DLLS "${SAPERA_SDK_PATH}/Bin/Win64/*.dll")
    foreach(DLL_PATH ${SAPERA_DLLS})
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DLL_PATH}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            COMMENT "Copying Sapera DLL: ${DLL_PATH}"
        )
    endforeach()
endif()

# Link with Qt libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
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
message(STATUS "Sapera SDK Found: ${SAPERA_FOUND}")
