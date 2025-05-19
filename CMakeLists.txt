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
set(CMAKE_CXX_EXTENSIONS ON)

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
option(BUILD_SHARED_LIBS "Build libraries as shared libraries" ON)
option(VOID_ARCHITECT_BUILD_TESTS "Build test executables" OFF)

set(OUTPUT_ROOT ${CMAKE_BINARY_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_ROOT}/bin/${CMAKE_SYSTEM_NAME})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_ROOT}/lib/${CMAKE_SYSTEM_NAME})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_ROOT}/lib/${CMAKE_SYSTEM_NAME})

# Assets configuration
set(ASSETS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(ASSETS_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets")

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
            set(COMPILED_SHADER "${ASSETS_OUTPUT_DIR}/shaders/${SHADER_NAME}.${SHADER_TYPE}.spv")

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

            add_custom_command(
                    OUTPUT ${COMPILED_SHADER}
                    COMMAND ${DXC_EXECUTABLE} -spirv -E main -T ${PROFILE} -Fo ${COMPILED_SHADER} ${SHADER_SOURCE}
                    DEPENDS ${SHADER_SOURCE}
                    COMMENT "Compiling ${SHADER_TYPE} shader: ${SHADER_FILENAME}"
            )

            # Add this shader to the custom target dependencies
            add_custom_target(
                    shader_${SHADER_NAME}_${SHADER_TYPE}
                    DEPENDS ${COMPILED_SHADER}
            )
            add_dependencies(compile_shaders shader_${SHADER_NAME}_${SHADER_TYPE})
        else ()
            message(WARNING "Shader file ${SHADER_FILENAME} does not follow the [name].[type].hlsl naming convention. Skipping.")
        endif ()
    endforeach ()
endfunction()

# Function to copy specified asset types
function(copy_assets SOURCE_SUBDIR OUTPUT_SUBDIR FILE_PATTERN)
    set(SOURCE_DIR "${ASSETS_SOURCE_DIR}/${SOURCE_SUBDIR}")
    set(OUTPUT_DIR "${ASSETS_OUTPUT_DIR}/${OUTPUT_SUBDIR}")

    file(GLOB_RECURSE ASSET_FILES "${SOURCE_DIR}/${FILE_PATTERN}")

    if (ASSET_FILES)
        # Create the output directory
        file(MAKE_DIRECTORY ${OUTPUT_DIR})

        # Create a custom target for copying these assets
        add_custom_target(copy_${SOURCE_SUBDIR}_assets ALL)

        foreach (ASSET_FILE ${ASSET_FILES})
            get_filename_component(ASSET_NAME ${ASSET_FILE} NAME)
            set(OUTPUT_FILE ${OUTPUT_DIR}/${ASSET_NAME})

            add_custom_command(
                    OUTPUT ${OUTPUT_FILE}
                    COMMAND ${CMAKE_COMMAND} -E copy ${ASSET_FILE} "${OUTPUT_DIR}/${ASSET_NAME}"
                    DEPENDS ${ASSET_FILE}
                    COMMENT "Copying asset: ${ASSET_NAME}"
            )

            # Add this asset to the custom target dependencies
            add_custom_target(
                    copy_${SOURCE_SUBDIR}_${ASSET_NAME}
                    DEPENDS ${OUTPUT_FILE}
            )
            add_dependencies(copy_${SOURCE_SUBDIR}_assets copy_${SOURCE_SUBDIR}_${ASSET_NAME})
        endforeach ()
    endif ()
endfunction()

# Call the setup functions to prepare asset directories
setup_asset_directories()

# Compile shaders
compile_hlsl_shaders()

# Add subprojects
add_subdirectory(engine)
add_subdirectory(client)

# Make the client depend on our asset compilation
add_dependencies(VoidArchitect_Client compile_shaders)

# Uncomment and adapt these lines when you need to copy other asset types
# copy_assets("textures" "textures" "*.png;*.jpg;*.jpeg")