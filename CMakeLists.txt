cmake_minimum_required(VERSION 3.15)

# Project name and version
project(CamMatrixCapture VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find Qt packages - use MSVC version
set(CMAKE_PREFIX_PATH "C:/Qt/6.9.0/msvc2022_64")
find_package(Qt6 REQUIRED COMPONENTS 
    Core
    Gui
    Widgets
)

# Add compiler warnings and options for MSVC
if(MSVC)
    add_compile_options(/W4 /Zc:__cplusplus)
    # Enable multi-processor compilation
    add_compile_options(/MP)
    # Enable exceptions
    add_compile_options(/EHsc)
    # Enable RTTI
    add_compile_options(/GR)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/gui/gui_manager.cpp
    src/gui/main_window.cpp
)

# Header files
set(HEADERS
    src/gui/gui_manager.hpp
    src/gui/main_window.hpp
)

# Create executable
add_executable(${PROJECT_NAME} 
    ${SOURCES}
    ${HEADERS}
)

# Set include directories for the target
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link Qt libraries
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

# Enable automoc for Qt
set_target_properties(${PROJECT_NAME} PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
)

# Add compiler definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# Install targets
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Print configuration summary
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Output Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "Qt Version: ${Qt6_VERSION}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}") 