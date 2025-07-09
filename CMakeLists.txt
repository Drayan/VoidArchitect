cmake_minimum_required(VERSION 3.16)

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

add_compile_definitions(VA_ENABLE_ASSERTS)

# Debug-specific settings
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
if (MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Od")
else ()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
endif ()

# Release-specific settings
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DFORCE_VALIDATION")

# === Build Configuration Options ===

# Core build options
option(BUILD_SHARED_LIBS "Build libraries as shared libraries" ON)
option(VOID_ARCHITECT_BUILD_TESTS "Build test executables" OFF)

# Engine modules build options
option(VOID_ARCHITECT_BUILD_CLIENT "Build client engine module" OFF)
option(VOID_ARCHITECT_BUILD_SERVER "Build server engine module" OFF)
option(VOID_ARCHITECT_BUILD_EDITOR "Build editor engine module" OFF)

# Application build options
option(VOID_ARCHITECT_BUILD_CLIENT_APP "Build client application" OFF)
option(VOID_ARCHITECT_BUILD_EDITOR_APP "Build editor application" OFF)

# Server service build options
option(VOID_ARCHITECT_BUILD_SERVER_FRONTEND "Build server frontend service" OFF)
option(VOID_ARCHITECT_BUILD_SERVER_GAME "Build server game logic service" OFF)

# Development and advanced options
option(VOID_ARCHITECT_ENABLE_HOT_RELOAD "Enable hot-reload support" ON)
option(VOID_ARCHITECT_ENABLE_PROFILING "Enable profiling support" OFF)
option(VOID_ARCHITECT_ENABLE_GPU_DEBUG "Enable GPU debugging layers" OFF)

# Set library type variable for engine modules
if (BUILD_SHARED_LIBS)
    set(VOID_ARCHITECT_LIB_TYPE SHARED)
else ()
    set(VOID_ARCHITECT_LIB_TYPE STATIC)
endif ()

# Validate configuration dependencies
if (VOID_ARCHITECT_BUILD_CLIENT_APP AND NOT VOID_ARCHITECT_BUILD_CLIENT)
    message(WARNING "Client application requires client engine module. Enabling VOID_ARCHITECT_BUILD_CLIENT.")
    set(VOID_ARCHITECT_BUILD_CLIENT ON CACHE BOOL "Build client engine module" FORCE)
endif ()

if (VOID_ARCHITECT_BUILD_EDITOR_APP AND NOT VOID_ARCHITECT_BUILD_EDITOR)
    message(WARNING "Editor application requires editor engine module. Enabling VOID_ARCHITECT_BUILD_EDITOR.")
    set(VOID_ARCHITECT_BUILD_EDITOR ON CACHE BOOL "Build editor engine module" FORCE)
endif ()

# Check if any server service is enabled and ensure server module is built
set(ANY_SERVER_SERVICE_ENABLED OFF)
if (VOID_ARCHITECT_BUILD_SERVER_FRONTEND OR VOID_ARCHITECT_BUILD_SERVER_GAME)
    set(ANY_SERVER_SERVICE_ENABLED ON)
endif ()

if (ANY_SERVER_SERVICE_ENABLED AND NOT VOID_ARCHITECT_BUILD_SERVER)
    message(WARNING "Server services require server engine module. Enabling VOID_ARCHITECT_BUILD_SERVER.")
    set(VOID_ARCHITECT_BUILD_SERVER ON CACHE BOOL "Build server engine module" FORCE)
endif ()

# === Output configuration ===

set(OUTPUT_ROOT ${CMAKE_BINARY_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_ROOT}/bin/${CMAKE_SYSTEM_NAME})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_ROOT}/lib/${CMAKE_SYSTEM_NAME})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_ROOT}/lib/${CMAKE_SYSTEM_NAME})

# === Shader Compilation System ===

# Assets configuration
set(ASSETS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(ASSETS_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")

# Find DXC executable from Vulkan SDK
find_package(Vulkan REQUIRED COMPONENTS dxc)
find_program(DXC_EXECUTABLE
        NAMES dxc
        HINTS ${Vulkan_INCLUDE_DIR}/../bin
        PATHS $ENV{VULKAN_SDK}/bin
        DOC "DirectX Shader Compiler"
)

if (NOT DXC_EXECUTABLE)
    message(FATAL_ERROR "Could not find DXC executable. Make sure Vulkan SDK is installed with DXC component.")
endif ()

message(STATUS "Found DXC compiler: ${DXC_EXECUTABLE}")

# Define shader compilation functions
function(setup_asset_directories)
    file(MAKE_DIRECTORY ${ASSETS_OUTPUT_DIR}/shaders)
endfunction()

# Create custom target for shader compilation
add_custom_target(compile_shaders ALL)

# Define shader compilation function
function(compile_hlsl_shaders)
    file(GLOB_RECURSE SHADER_SOURCES "${ASSETS_SOURCE_DIR}/shaders/*.hlsl")

    # Process each shader file
    foreach (SHADER_SOURCE ${SHADER_SOURCES})
        get_filename_component(SHADER_FILENAME ${SHADER_SOURCE} NAME)

        # Parse the filename to extract parts according to [name].[type].hlsl convention
        string(REGEX MATCH "^(.+)\\.([^.]+)\\.hlsl$" SHADER_PARTS ${SHADER_FILENAME})

        if (SHADER_PARTS)
            set(SHADER_NAME ${CMAKE_MATCH_1})
            set(SHADER_TYPE ${CMAKE_MATCH_2})
            set(COMPILED_SHADER "${ASSETS_OUTPUT_DIR}/shaders/${SHADER_NAME}.${SHADER_TYPE}.shader")
            set(YAML_CONFIG "${ASSETS_OUTPUT_DIR}/shaders/${SHADER_NAME}.${SHADER_TYPE}.yaml")

            # Map shader type to appropriate DXC target profiles
            if (SHADER_TYPE STREQUAL "vert")
                set(PROFILE "vs_6_0")
            elseif (SHADER_TYPE STREQUAL "pixl")
                set(PROFILE "ps_6_0")
            elseif (SHADER_TYPE STREQUAL "geom")
                set(PROFILE "gs_6_0")
            elseif (SHADER_TYPE STREQUAL "comp")
                set(PROFILE "cs_6_0")
            elseif (SHADER_TYPE STREQUAL "hull")
                set(PROFILE "hs_6_0")
            elseif (SHADER_TYPE STREQUAL "domn")
                set(PROFILE "ds_6_0")
            elseif (SHADER_TYPE STREQUAL "mesh")
                set(PROFILE "ms_6_0")
            elseif (SHADER_TYPE STREQUAL "ampl")
                set(PROFILE "as_6_0")
            else ()
                message(WARNING "Unknown shader type : ${SHADER_TYPE}, trying with vs_6_0")
                SET(PROFILE "vs_6_0")
            endif ()

            # Create a Python script for extracting YAML
            # TEMP This will be integrated later on in a custom shader tool
            set(EXTRACT_SCRIPT "${CMAKE_BINARY_DIR}/extract_yaml_header.py")
            # Extract for extracting YAML header

            add_custom_command(
                    OUTPUT ${YAML_CONFIG}
                    COMMAND ${CMAKE_COMMAND} -E env python3 ${EXTRACT_SCRIPT} ${SHADER_SOURCE} ${YAML_CONFIG} || ${CMAKE_COMMAND} -E touch "${YAML_CONFIG}"
                    DEPENDS ${SHADER_SOURCE} ${EXTRACT_SCRIPT}
                    COMMENT "Extracting YAML header from ${SHADER_FILENAME}"
            )

            # Compiling the shader source
            add_custom_command(
                    OUTPUT ${COMPILED_SHADER}
                    COMMAND ${DXC_EXECUTABLE} -spirv -E main -T ${PROFILE} -Fo ${COMPILED_SHADER} ${SHADER_SOURCE}
                    DEPENDS ${SHADER_SOURCE}
                    COMMENT " Compiling ${SHADER_TYPE} shader: ${SHADER_FILENAME}"
            )

            # Add this shader to the custom target dependencies
            add_custom_target(
                    shader_${SHADER_NAME}_${SHADER_TYPE}
                    DEPENDS ${COMPILED_SHADER} ${YAML_CONFIG}
            )
            add_dependencies(compile_shaders shader_${SHADER_NAME}_${SHADER_TYPE})
        else ()
            message(WARNING "Shader file ${SHADER_FILENAME} does not follow the [name].[type].hlsl naming convention. Skipping.")
        endif ()
    endforeach ()
endfunction()

# Call the setup functions to prepare asset directories
setup_asset_directories()
compile_hlsl_shaders()

# === Project Structure ===

# Engine module (always built)
add_subdirectory(engine)

# Applications
#if (VOID_ARCHITECT_BUILD_CLIENT_APP AND VOID_ARCHITECT_BUILD_CLIENT)
#    add_subdirectory(apps/client)
#    add_dependencies(VoidArchitect_ClientApp compile_shaders)
#endif ()
#
#if (VOID_ARCHITECT_BUILD_EDITOR_APP AND VOID_ARCHITECT_BUILD_EDITOR)
#    add_subdirectory(apps/editor)
#    add_dependencies(VoidArchitect_EditorApp compile_shaders)
#endif ()
#
#if (ANY_SERVER_SERVICE_ENABLED AND VOID_ARCHITECT_BUILD_SERVER)
#    add_subdirectory(apps/server)
#endif ()
# Tests
if (VOID_ARCHITECT_BUILD_TESTS)
    add_subdirectory(tests)
endif ()

# === Configuration Summary ===

message(STATUS "")
message(STATUS "=== VoidArchitect Build Configuration ===")
message(STATUS "Version : ${PROJECT_VERSION}")
message(STATUS "Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "-----------------------------------------")
message(STATUS "Libraries:")
message(STATUS "  Shared Libraries: ${BUILD_SHARED_LIBS}")
message(STATUS "  Hot reload : ${VOID_ARCHITECT_ENABLE_HOT_RELOAD}")
message(STATUS "  Profiling : ${VOID_ARCHITECT_ENABLE_PROFILING}")
message(STATUS "  GPU Debug : ${VOID_ARCHITECT_ENABLE_GPU_DEBUG}")
message(STATUS "Engine Modules:")
message(STATUS "  Common: ON (always built)")
message(STATUS "  RHI: ON (always built)")
message(STATUS "  Client: ${VOID_ARCHITECT_BUILD_CLIENT}")
message(STATUS "  Server: ${VOID_ARCHITECT_BUILD_SERVER}")
message(STATUS "  Editor: ${VOID_ARCHITECT_BUILD_EDITOR}")
message(STATUS "Applications:")
message(STATUS "  Client App: ${VOID_ARCHITECT_BUILD_CLIENT_APP}")
message(STATUS "  Editor App: ${VOID_ARCHITECT_BUILD_EDITOR_APP}")
message(STATUS "Server Services:")
message(STATUS "  Frontend: ${VOID_ARCHITECT_BUILD_SERVER_FRONTEND}")
message(STATUS "  Game: ${VOID_ARCHITECT_BUILD_SERVER_GAME}")
message(STATUS "Development:")
message(STATUS "  Tests: ${VOID_ARCHITECT_BUILD_TESTS}")
message(STATUS "  DXC Compiler: ${DXC_EXECUTABLE}")
message(STATUS "==========================================")