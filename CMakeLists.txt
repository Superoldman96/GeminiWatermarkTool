cmake_minimum_required(VERSION 3.20)
project(GeminiWatermarkTool VERSION 0.1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# Build Options
# =============================================================================

# =============================================================================
# Application Metadata
# =============================================================================
set(APP_PRODUCT_NAME "Gemini Watermark Tool")
set(APP_DESCRIPTION "Add/Remove Gemini-style visible watermarks")
set(APP_COMPANY "Kwyshell")
set(APP_COPYRIGHT "Copyright (C) 2025 Allen Kuo. MIT License.")

# =============================================================================
# Platform Detection
# =============================================================================
if(WIN32)
    set(PLATFORM_NAME "Windows")
elseif(APPLE)
    set(PLATFORM_NAME "macOS")
elseif(ANDROID)
    set(PLATFORM_NAME "Android")
elseif(UNIX)
    set(PLATFORM_NAME "Linux")
else()
    set(PLATFORM_NAME "Unknown")
endif()

# =============================================================================
# Windows Configuration
# =============================================================================
if(WIN32)
    set(APP_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico")

    if(EXISTS "${APP_ICON_PATH}")
        # Generate .rc from template
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc.in"
            "${CMAKE_CURRENT_BINARY_DIR}/resources/app.rc"
            @ONLY
        )
        set(WIN_RESOURCE_FILE "${CMAKE_CURRENT_BINARY_DIR}/resources/app.rc")
        message(STATUS "Windows resource file configured with icon: ${APP_ICON_PATH}")
    else()
        message(STATUS "No icon found at ${APP_ICON_PATH}, skipping resource file")
        set(WIN_RESOURCE_FILE "")
    endif()

    # MSVC specific settings
    if(MSVC)
        # Use static runtime library (/MT instead of /MD)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

        # Compiler flags
        add_compile_options(/W4 /utf-8)

        # Release optimizations
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /GL /Gy")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /OPT:REF /OPT:ICF")
    endif()
endif()

# =============================================================================
# macOS Configuration
# =============================================================================
if(APPLE)
    # macOS minimum deployment target
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

    # Support both Intel and Apple Silicon (Universal Binary)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures")

    # Compiler warnings
    add_compile_options(-Wall -Wextra -Wpedantic)

    # Release optimizations
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -DNDEBUG)
    endif()

    # Link against macOS frameworks if needed
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    if(COREFOUNDATION_LIBRARY)
        message(STATUS "Found CoreFoundation: ${COREFOUNDATION_LIBRARY}")
    endif()
endif()

# =============================================================================
# Linux Configuration
# =============================================================================
if(UNIX AND NOT APPLE AND NOT ANDROID)
    # Compiler warnings
    add_compile_options(-Wall -Wextra -Wpedantic)

    # Release optimizations
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -DNDEBUG)
    endif()

    # Static linking options (for standalone distribution)
    # Try to link statically where possible
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")

    # Required for std::filesystem on older GCC
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
        link_libraries(stdc++fs)
    endif()
endif()

# =============================================================================
# Android Configuration
# =============================================================================
if(ANDROID)
    # Android specific settings
    message(STATUS "Building for Android: ABI=${ANDROID_ABI}, Platform=${ANDROID_PLATFORM}")

    # Compiler warnings
    add_compile_options(-Wall -Wextra)

    # Release optimizations
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -DNDEBUG)
    endif()

    # Android uses Bionic libc, no need for special filesystem handling
endif()

# =============================================================================
# Dependencies (via vcpkg)
# =============================================================================
find_package(OpenCV REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# =============================================================================
# Main Executable
# =============================================================================
set(SOURCES
    src/main.cpp
    src/watermark_engine.cpp
    src/blend_modes.cpp
)

set(HEADERS
    src/watermark_engine.hpp
    src/blend_modes.hpp
    src/ascii_logo.hpp
)

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${WIN_RESOURCE_FILE}  # Windows icon + version info (empty on non-Windows)
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${OpenCV_LIBS}
    fmt::fmt
    CLI11::CLI11
    spdlog::spdlog
)

# macOS framework linking
if(APPLE AND COREFOUNDATION_LIBRARY)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${COREFOUNDATION_LIBRARY})
endif()

# =============================================================================
# Compile Definitions
# =============================================================================
target_compile_definitions(${PROJECT_NAME} PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    APP_NAME="${PROJECT_NAME}"
)

# =============================================================================
# =============================================================================
# Strip binary for release (Linux/macOS/Android)
# =============================================================================
if(NOT WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_STRIP} $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Stripping binary for release"
    )
endif()
# =============================================================================
# Install
# =============================================================================
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# =============================================================================
# Configuration Summary
# =============================================================================
message(STATUS "")
message(STATUS "=== Gemini Watermark Tool Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Platform: ${PLATFORM_NAME}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "OpenCV: ${OpenCV_VERSION}")
if(APPLE)
    message(STATUS "macOS Architectures: ${CMAKE_OSX_ARCHITECTURES}")
    message(STATUS "macOS Deployment Target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()
if(ANDROID)
    message(STATUS "Android ABI: ${ANDROID_ABI}")
    message(STATUS "Android Platform: ${ANDROID_PLATFORM}")
endif()
message(STATUS "=============================================")

