project(VoidArchitectEngine_Client VERSION 0.1.0 LANGUAGES CXX)
include(FetchContent)

if (NOT TARGET SDL3)
    find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
endif ()

if (NOT TARGET stb)
    FetchContent_Declare(
            stb
            URL https://github.com/nothings/stb/archive/master.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(stb)
endif ()

if (NOT TARGET assimp)
    find_package(assimp CONFIG REQUIRED)
endif ()

if (NOT TARGET yaml-cpp)
    FetchContent_Declare(
            yaml-cpp
            GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
            GIT_TAG 0.8.0
    )
    FetchContent_MakeAvailable(yaml-cpp)
endif ()

if (NOT TARGET lz4)
    find_package(lz4 CONFIG REQUIRED)
endif ()

# Create STB interface library
add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE ${stb_SOURCE_DIR})

# List source files
file(GLOB_RECURSE CLIENT_SOURCES "src/*.cpp")
file(GLOB_RECURSE CLIENT_HEADERS "src/*.hpp")

# Create the Client lib
add_library(VoidArchitectEngine_Client STATIC
        ${CLIENT_SOURCES}
        ${CLIENT_HEADERS}
)

# Client dependencies
target_link_libraries(VoidArchitectEngine_Client PUBLIC
        VoidArchitectEngine_Common
        VoidArchitectEngine_RHI_Interface
        VoidArchitectEngine_RHI_Vulkan
        SDL3::SDL3
        stb_image
        assimp::assimp
        yaml-cpp::yaml-cpp
        lz4::lz4
)

# Include directories
target_include_directories(VoidArchitectEngine_Client PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/VoidArchitect/Engine")

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_BINARY_DIR}/include/VoidArchitect/Engine/Client"
)

# Platform-specific configuration
if (APPLE)
    target_compile_definitions(VoidArchitectEngine_Client PRIVATE "VOID_ARCH_MACOS")
    set_target_properties(VoidArchitectEngine_Client PROPERTIES
            INSTALL_RPATH "@executable_path/../../lib/${CMAKE_SYSTEM_NAME}"
            BUILD_WITH_INSTALL_RPATH TRUE
    )
elseif (WIN32)
    target_compile_definitions(VoidArchitectEngine_Client PRIVATE "VOID_ARCH_WINDOWS")
elseif (UNIX AND NOT APPLE)
    target_compile_definitions(VoidArchitectEngine_Client PRIVATE "VOID_ARCH_LINUX")
    set_target_properties(VoidArchitectEngine_Client PROPERTIES
            INSTALL_RPATH "$ORIGIN/../../lib/${CMAKE_SYSTEM_NAME}"
            BUILD_WITH_INSTALL_RPATH TRUE
    )
endif ()

# Define export macros for shared library
if (VOID_ARCHITECT_SHARED_LIBS)
    target_compile_definitions(VoidArchitectEngine_Client PRIVATE VOID_ARCH_CLIENT_EXPORTS)
    target_compile_definitions(VoidArchitectEngine_Client INTERFACE VOID_ARCH_CLIENT_IMPORTS)
endif ()

target_precompile_headers(VoidArchitectEngine_Client PRIVATE "src/client_pch.hpp")

# Installation
install(TARGETS VoidArchitectEngine_Client
        EXPORT VoidArchitectEngine_ClientTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
)

install(DIRECTORY src/
        DESTINATION include/VoidArchitect/Engine/Client
        FILES_MATCHING PATTERN "*.hpp"
)