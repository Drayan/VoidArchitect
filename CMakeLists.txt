cmake_minimum_required(VERSION 3.10)

project(VoidArchitect VERSION 0.1.0 LANGUAGES CXX)

# Set default build type if not specified
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif ()
message(STATUS "Build type : ${CMAKE_BUILD_TYPE}")

# Compilation options
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform detection
if (APPLE)
    message(STATUS "Configuring for macOS")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures to build for")
    set(CMAKE_MACOSX_RPATH ON)
elseif (WIN32)
    message(STATUS "Configuring for Windows")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
elseif (UNIX AND NOT APPLE)
    message(STATUS "Setting up for linux")
endif ()

# Position Independent Code - required for shared libraries on all platforms
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Platform-specific compiler flags
if (APPLE)
    add_compile_options(-Wall -Wextra)
elseif (MSVC)
    add_compile_options(/W4)
else ()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif ()

# Debug-specific settings
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
if (MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Od")
else ()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
endif ()

# Release-specific settings
option(BUILD_SHARED_LIBS "Build libraries as shared libraries" ON)
option(VOID_ARCHITECT_BUILD_TESTS "Build test executables" OFF)

set(OUTPUT_ROOT ${CMAKE_BINARY_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_ROOT}/bin/${CMAKE_SYSTEM_NAME})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_ROOT}/lib/${CMAKE_SYSTEM_NAME})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_ROOT}/lib/${CMAKE_SYSTEM_NAME})

# Add subprojects
add_subdirectory(engine_client)
add_subdirectory(client)