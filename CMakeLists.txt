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

# Check for Sapera SDK - use environment variable to find it
if(DEFINED ENV{SAPERADIR})
    set(SAPERA_DIR $ENV{SAPERADIR})
    message(STATUS "Found Sapera SDK at: ${SAPERA_DIR}")

    set(SAPERA_INCLUDE_DIRS
        "${SAPERA_DIR}/Classes/Basic"
        "${SAPERA_DIR}/Classes"
        "${SAPERA_DIR}/Include"
        "${SAPERA_DIR}/Classes/Gui"
    )

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        # 64-bit build
        set(SAPERA_LIB_DIRS
            "${SAPERA_DIR}/Lib"
            "${SAPERA_DIR}/Lib/Win64"
            "${SAPERA_DIR}/Lib/Win64/VS2019"
        )
    else()
        # 32-bit build
        set(SAPERA_LIB_DIRS
            "${SAPERA_DIR}/Lib"
            "${SAPERA_DIR}/Lib/Win32"
            "${SAPERA_DIR}/Lib/Win32/VS2019"
        )
    endif()

    # Define SAPERA_FOUND for conditional compilation
    add_compile_definitions(SAPERA_FOUND)

    # Add Sapera libraries to link
    set(SAPERA_LIBS corapi SapClassBasic)
else()
    message(STATUS "Sapera SDK not found - using mock cameras only")

    # Define flag to conditionally compile code without Sapera
    add_compile_definitions(SAPERA_NOT_FOUND)

    # Let's filter out Sapera-related source files if SDK is not available
    list(FILTER SOURCES EXCLUDE REGEX ".*sapera.*\\.cpp$")
    list(FILTER HEADERS EXCLUDE REGEX ".*sapera.*\\.hpp$")
endif()

# Automatically find all source and header files in src directory
file(GLOB_RECURSE SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
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

# Add Sapera includes if found
if(DEFINED ENV{SAPERADIR})
    target_include_directories(${PROJECT_NAME} PRIVATE ${SAPERA_INCLUDE_DIRS})
    target_link_directories(${PROJECT_NAME} PRIVATE ${SAPERA_LIB_DIRS})
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# Add Sapera libraries if found
if(DEFINED ENV{SAPERADIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${SAPERA_LIBS})
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
