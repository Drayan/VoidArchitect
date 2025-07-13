project(VoidArchitectEngine_Common VERSION 0.1.0 LANGUAGES CXX)
include(FetchContent)

# Find required dependencies for Common module
if (NOT TARGET spdlog)
    find_package(spdlog REQUIRED)
endif ()

if (NOT TARGET unofficial-concurrentqueue)
    find_package(unofficial-concurrentqueue CONFIG REQUIRED)
endif ()

if (NOT TARGET glm)
    find_package(glm CONFIG REQUIRED)
endif ()

if (NOT TARGET SDL3)
    find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
endif ()

# List source files
file(GLOB_RECURSE COMMON_SOURCES "src/*.cpp")
file(GLOB_RECURSE COMMON_HEADERS "src/*.hpp")

# Create the Common library
add_library(VoidArchitectEngine_Common ${VOID_ARCHITECT_LIB_TYPE}
        ${COMMON_SOURCES}
        ${COMMON_HEADERS}
)

# Link dependencies
target_link_libraries(VoidArchitectEngine_Common PUBLIC
        spdlog::spdlog
        glm::glm
        unofficial::concurrentqueue::concurrentqueue
        SDL3::SDL3
)

# Include directories
target_include_directories(VoidArchitectEngine_Common PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/VoidArchitect/Engine")

execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_BINARY_DIR}/include/VoidArchitect/Engine/Common"
)

# Platform-specific configuration
if (APPLE)
    target_compile_definitions(VoidArchitectEngine_Common PRIVATE "VOID_ARCH_MACOS")
    set_target_properties(VoidArchitectEngine_Common PROPERTIES
            INSTALL_RPATH "@executable_path/../../lib/${CMAKE_SYSTEM_NAME}"
            BUILD_WITH_INSTALL_RPATH TRUE
    )
elseif (WIN32)
    target_compile_definitions(VoidArchitectEngine_Common PRIVATE "VOID_ARCH_WINDOWS")
elseif (UNIX AND NOT APPLE)
    target_compile_definitions(VoidArchitectEngine_Common PRIVATE "VOID_ARCH_LINUX")
    set_target_properties(VoidArchitectEngine_Common PROPERTIES
            INSTALL_RPATH "$ORIGIN/../../lib/${CMAKE_SYSTEM_NAME}"
            BUILD_WITH_INSTALL_RPATH TRUE
    )
endif ()

# Define export macros for shared library
if (VOID_ARCHITECT_SHARED_LIBS)
    target_compile_definitions(VoidArchitectEngine_Common PRIVATE VOID_ARCH_COMMON_EXPORTS)
    target_compile_definitions(VoidArchitectEngine_Common INTERFACE VOID_ARCH_COMMON_IMPORTS)
endif ()

# Precompiled headers
target_precompile_headers(VoidArchitectEngine_Common PRIVATE "src/common_pch.hpp")

# Installation
install(TARGETS VoidArchitectEngine_Common
        EXPORT VoidArchitectEngine_CommonTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY src/
        DESTINATION include/VoidArchitect/Engine/Common
        FILES_MATCHING PATTERN "*.hpp"
)