project(VoidArchitect_EngineClient VERSION 0.1.0 LANGUAGES CXX)

# List source files
file(GLOB_RECURSE ENGINE_CLIENT_SOURCES "src/*.cpp")
file(GLOB_RECURSE ENGINE_CLIENT_HEADERS "src/*.hpp")

if (NOT TARGET spdlog)
    find_package(spdlog REQUIRED)
endif ()

if (NOT TARGET SDL3)
    find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
endif ()

if (NOT TARGET Vulkan)
    find_package(Vulkan REQUIRED COMPONENTS MoltenVK)
endif ()

# Create the library
add_library(VoidArchitect_EngineClient
        ${ENGINE_CLIENT_SOURCES}
        ${ENGINE_CLIENT_HEADERS}
)
target_link_libraries(VoidArchitect_EngineClient PUBLIC spdlog::spdlog SDL3::SDL3 Vulkan::Vulkan Vulkan::MoltenVK)

# If the compiler support PCH, use it.
target_precompile_headers(VoidArchitect_EngineClient PRIVATE "src/va.pch.hpp")
target_compile_options(VoidArchitect_EngineClient PRIVATE "-Winvalid-pch")

set_target_properties(VoidArchitect_EngineClient PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(VoidArchitect_EngineClient PROPERTIES LINKER_LANGUAGE CXX)

# Platform-specific configuration
if (APPLE)
    # macOS settings
    target_compile_definitions(VoidArchitect_EngineClient PRIVATE "VOID_ARCH_MACOS")

    get_filename_component(VULKAN_LIBRARY_PATH "${Vulkan_LIBRARY}" DIRECTORY)
    set_target_properties(VoidArchitect_EngineClient PROPERTIES
            INSTALL_RPATH "@executable_path/../../lib/${CMAKE_SYSTEM_NAME};${VULKAN_LIBRARY_PATH}"
            BUILD_WITH_INSTALL_RPATH TRUE
    )
elseif (WIN32)
    # Windows settings
    target_compile_definitions(VoidArchitect_EngineClient PRIVATE "VOID_ARCH_WINDOWS")
elseif (UNIX AND NOT APPLE)
    # Linux settings
    target_compile_definitions(VoidArchitect_EngineClient PRIVATE "VOID_ARCH_LINUX")
    set_target_properties(VoidArchitect_EngineClient PROPERTIES
            INSTALL_RPATH "$ORIGIN/../../lib/${CMAKE_SYSTEM_NAME}"
            BUILD_WITH_INSTALL_RPATH TRUE
    )
endif ()

# Define export macros
target_compile_definitions(VoidArchitect_EngineClient PRIVATE VOID_ARCH_ENGINE_EXPORTS)

# Include directories
target_include_directories(VoidArchitect_EngineClient
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

# Installation settings
install(TARGETS VoidArchitect_EngineClient
        EXPORT VoidArchitect_EngineClientTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY src/
        DESTINATION include/VoidArchitect_EngineClient
        FILES_MATCHING PATTERN "*.hpp"
)

# Test configuration
if (VOID_ARCHITECT_BUILD_TESTS)
    add_subdirectory(tests)
endif ()