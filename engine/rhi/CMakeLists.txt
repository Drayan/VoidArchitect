project(VoidArchitectEngine_RHI VERSION 0.1.0 LANGUAGES CXX)

# Find Vulkan dependency
if (NOT TARGET Vulkan)
    find_package(Vulkan REQUIRED COMPONENTS MoltenVK)
endif ()

# === RHI Interface Library ===
file(GLOB_RECURSE RHI_INTERFACE_SOURCES "src/Interface/*.cpp")
file(GLOB_RECURSE RHI_INTERFACE_HEADERS "src/Interface/*.hpp")

# Add Resources sources
file(GLOB_RECURSE RHI_RESOURCES_SOURCES "src/Resources/*.cpp")
file(GLOB_RECURSE RHI_RESOURCES_HEADERS "src/Resources/*.hpp")

# Add Systems sources for plugin management
file(GLOB_RECURSE RHI_LOADER_SOURCES "src/Systems/*.cpp")
file(GLOB_RECURSE RHI_LOADER_HEADERS "src/Systems/*.hpp")

# Create RHI Interface library
add_library(VoidArchitectEngine_RHI_Interface ${VOID_ARCHITECT_LIB_TYPE}
        ${RHI_INTERFACE_SOURCES}
        ${RHI_INTERFACE_HEADERS}
        ${RHI_RESOURCES_SOURCES}
        ${RHI_RESOURCES_HEADERS}
        ${RHI_LOADER_SOURCES}
        ${RHI_LOADER_HEADERS}
)

# RHI Interface depends on Common
target_link_libraries(VoidArchitectEngine_RHI_Interface PUBLIC
        VoidArchitectEngine_Common
)

# Include directories for Interface
target_include_directories(VoidArchitectEngine_RHI_Interface PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/VoidArchitect/Engine")

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_BINARY_DIR}/include/VoidArchitect/Engine/RHI"
)

target_precompile_headers(VoidArchitectEngine_RHI_Interface PRIVATE "src/rhi_pch.hpp")

# === Platform Detection and Backend Configuration ===

# Detect Vulkan availability
find_package(Vulkan QUIET)
if (Vulkan_FOUND)
    set(VOID_ARCH_RHI_VULKAN_AVAILABLE ON)
    message(STATUS "Vulkan found: enabling Vulkan RHI backend.")
else ()
    set(VOID_ARCH_RHI_VULKAN_AVAILABLE OFF)
    message(STATUS "Vulkan not found: disabling Vulkan RHI backend.")
endif ()

if (WIN32)
    set(VOID_ARCH_RHI_DIRECTX12_AVAILABLE OFF)
endif ()

set(VOID_ARCH_RHI_OPENGL_AVAILABLE OFF)

if (APPLE)
    set(VOID_ARCH_RHI_METAL_AVAILABLE OFF)
endif ()

# === Vulkan RHI Plugin ===
file(GLOB_RECURSE RHI_VULKAN_SOURCES "src/Vulkan/*.cpp")
file(GLOB_RECURSE RHI_VULKAN_HEADERS "src/Vulkan/*.hpp")

# Create Vulkan RHI plugin
add_library(VoidArchitectEngine_RHI_Vulkan STATIC
        ${RHI_VULKAN_SOURCES}
        ${RHI_VULKAN_HEADERS}
)

# Vulkan plugin dependencies
target_link_libraries(VoidArchitectEngine_RHI_Vulkan PUBLIC
        VoidArchitectEngine_Client
        Vulkan::Vulkan
        Vulkan::MoltenVK
)

# Include directories for Vulkan plugin
target_include_directories(VoidArchitectEngine_RHI_Vulkan PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Platform-specific configuration
if (APPLE)
    get_filename_component(VULKAN_LIBRARY_PATH "${Vulkan_LIBRARY}" DIRECTORY)
    set_target_properties(VoidArchitectEngine_RHI_Interface PROPERTIES
            INSTALL_RPATH "@executable_path/../../lib/${CMAKE_SYSTEM_NAME};${VULKAN_LIBRARY_PATH}"
            BUILD_WITH_INSTALL_RPATH TRUE
    )
    set_target_properties(VoidArchitectEngine_RHI_Vulkan PROPERTIES
            INSTALL_RPATH "@executable_path/../../lib/${CMAKE_SYSTEM_NAME};${VULKAN_LIBRARY_PATH}"
            BUILD_WITH_INSTALL_RPATH TRUE
    )
elseif (UNIX AND NOT APPLE)
    set_target_properties(VoidArchitectEngine_RHI_Interface PROPERTIES
            INSTALL_RPATH "$ORIGIN/../../lib/${CMAKE_SYSTEM_NAME}"
            BUILD_WITH_INSTALL_RPATH TRUE
    )
    set_target_properties(VoidArchitectEngine_RHI_Vulkan PROPERTIES
            INSTALL_RPATH "$ORIGIN/../../lib/${CMAKE_SYSTEM_NAME}"
            BUILD_WITH_INSTALL_RPATH TRUE
    )
endif ()

# Define export macros
if (VOID_ARCHITECT_SHARED_LIBS)
    target_compile_definitions(VoidArchitectEngine_RHI_Interface PRIVATE VOID_ARCH_RHI_EXPORTS)
    target_compile_definitions(VoidArchitectEngine_RHI_Interface INTERFACE VOID_ARCH_RHI_IMPORTS)
endif ()

target_compile_definitions(VoidArchitectEngine_RHI_Interface PUBLIC
        VOID_ARCH_RHI_VULKAN_AVAILABLE
)

target_compile_definitions(VoidArchitectEngine_RHI_Vulkan PRIVATE VOID_ARCH_RHI_VULKAN_EXPORTS)

target_precompile_headers(VoidArchitectEngine_RHI_Vulkan REUSE_FROM VoidArchitectEngine_RHI_Interface)

# Installation
install(TARGETS VoidArchitectEngine_RHI_Interface VoidArchitectEngine_RHI_Vulkan
        EXPORT VoidArchitectEngine_RHITargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY src/Interface/ src/Systems/ src/Resources src/Vulkan
        DESTINATION include/VoidArchitect/Engine/RHI
        FILES_MATCHING PATTERN "*.hpp"
)