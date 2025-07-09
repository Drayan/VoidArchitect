project(VoidArchitectEngine_RHI VERSION 0.1.0 LANGUAGES CXX)

# Find Vulkan dependency
if (NOT TARGET Vulkan)
    find_package(Vulkan REQUIRED COMPONENTS MoltenVK)
endif ()

# === RHI Interface Library ===
file(GLOB_RECURSE RHI_INTERFACE_SOURCES "src/interface/*.cpp")
file(GLOB_RECURSE RHI_INTERFACE_HEADERS "src/interface/*.hpp")

# Add loader sources for plugin management
file(GLOB_RECURSE RHI_LOADER_SOURCES "src/loader/*.cpp")
file(GLOB_RECURSE RHI_LOADER_HEADERS "src/loader/*.hpp")

# Create RHI Interface library
add_library(VoidArchitectEngine_RHI_Interface ${VOID_ARCHITECT_LIB_TYPE}
        ${RHI_INTERFACE_SOURCES}
        ${RHI_INTERFACE_HEADERS}
        ${RHI_LOADER_SOURCES}
        ${RHI_LOADER_HEADERS}
)

# RHI Interface depends on Common
target_link_libraries(VoidArchitectEngine_RHI_Interface PUBLIC
        VoidArchitectEngine_Common
)

# Include directories for Interface
target_include_directories(VoidArchitectEngine_RHI_Interface PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include/VoidArchitectEngine_RHI>
)

# === Vulkan RHI Plugin ===
file(GLOB_RECURSE RHI_VULKAN_SOURCES "src/vulkan/*.cpp")
file(GLOB_RECURSE RHI_VULKAN_HEADERS "src/vulkan/*.hpp")

# Create Vulkan RHI plugin
add_library(VoidArchitect_RHI_Vulkan SHARED
        ${RHI_VULKAN_SOURCES}
        ${RHI_VULKAN_HEADERS}
)

# Vulkan plugin dependencies
target_link_libraries(VoidArchitectEngine_RHI_Vulkan PUBLIC
        VoidArchitectEngine_RHI_Interface
        Vulkan::Vulkan
        Vulkan::MoltenVK
)

# Include directories for Vulkan plugin
target_include_directories(VoidArchitect_RHI_Vulkan PRIVATE
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

target_compile_definitions(VoidArchitect_RHI_Vulkan PRIVATE VOID_ARCH_RHI_VULKAN_EXPORTS)

# Installation
install(TARGETS VoidArchitectEngine_RHI_Interface VoidArchitect_RHI_Vulkan
        EXPORT VoidArchitectEngine_RHITargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY src/interface/ src/loader/
        DESTINATION include/VoidArchitectEngine_RHI
        FILES_MATCHING PATTERN "*.hpp"
)