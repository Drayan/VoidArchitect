project(VoidArchitect_Engine VERSION 0.1.0 LANGUAGES CXX)
include(FetchContent)
# List source files
file(GLOB_RECURSE ENGINE_SOURCES "src/*.cpp")
file(GLOB_RECURSE ENGINE_HEADERS "src/*.hpp")

if (NOT TARGET spdlog)
    find_package(spdlog REQUIRED)
endif ()

if (NOT TARGET SDL3)
    find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
endif ()

if (NOT TARGET stb)
    FetchContent_Declare(
            stb
            URL https://github.com/nothings/stb/archive/master.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(stb)
endif ()

if (NOT TARGET assimp)
    find_package(assimp CONFIG REQUIRED)
endif ()

if (NOT TARGET unofficial-concurrentqueue)
    find_package(unofficial-concurrentqueue CONFIG REQUIRED)
endif ()

if (NOT TARGET yaml-cpp)
    FetchContent_Declare(
            yaml-cpp
            GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
            GIT_TAG 0.8.0
    )
    FetchContent_MakeAvailable(yaml-cpp)
endif ()

if (NOT TARGET Vulkan)
    find_package(Vulkan REQUIRED COMPONENTS MoltenVK)
endif ()

if (NOT TARGET glm)
    find_package(glm REQUIRED CONFIG)
endif ()

if (NOT TARGET lz4)
    find_package(lz4 CONFIG REQUIRED)
endif ()

add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE ${stb_SOURCE_DIR})

# Create the library
add_library(VoidArchitect_Engine
        ${ENGINE_SOURCES}
        ${ENGINE_HEADERS}
)
target_link_libraries(VoidArchitect_Engine PUBLIC spdlog::spdlog SDL3::SDL3 stb_image assimp::assimp lz4::lz4 yaml-cpp::yaml-cpp Vulkan::Vulkan Vulkan::MoltenVK glm::glm unofficial::concurrentqueue::concurrentqueue)

# If the compiler support PCH, use it.
target_precompile_headers(VoidArchitect_Engine PRIVATE "src/va.pch.hpp")
target_compile_options(VoidArchitect_Engine PRIVATE "-Winvalid-pch")

set_target_properties(VoidArchitect_Engine PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(VoidArchitect_Engine PROPERTIES LINKER_LANGUAGE CXX)

# Platform-specific configuration
if (APPLE)
    # macOS settings
    #    target_compile_definitions(VoidArchitect_Engine PRIVATE "VOID_ARCH_PLATFORM_MACOS")

    get_filename_component(VULKAN_LIBRARY_PATH "${Vulkan_LIBRARY}" DIRECTORY)
    set_target_properties(VoidArchitect_Engine PROPERTIES
            INSTALL_RPATH "@executable_path/../../lib/${CMAKE_SYSTEM_NAME};${VULKAN_LIBRARY_PATH}"
            BUILD_WITH_INSTALL_RPATH TRUE
    )
elseif (WIN32)
    # Windows settings
    #    target_compile_definitions(VoidArchitect_Engine PRIVATE "VOID_ARCH_PLATFORM_WINDOWS")
elseif (UNIX AND NOT APPLE)
    # Linux settings
    #    target_compile_definitions(VoidArchitect_Engine PRIVATE "VOID_ARCH_PLATFORM_LINUX")
    set_target_properties(VoidArchitect_Engine PROPERTIES
            INSTALL_RPATH "$ORIGIN/../../lib/${CMAKE_SYSTEM_NAME}"
            BUILD_WITH_INSTALL_RPATH TRUE
    )
endif ()

# Define export macros
target_compile_definitions(VoidArchitect_Engine PRIVATE VOID_ARCH_ENGINE_EXPORTS)

# Include directories
target_include_directories(VoidArchitect_Engine
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

# Installation settings
install(TARGETS VoidArchitect_Engine
        EXPORT VoidArchitect_EngineTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY src/
        DESTINATION include/VoidArchitect_Engine
        FILES_MATCHING PATTERN "*.hpp"
)

# Test configuration
if (VOID_ARCHITECT_BUILD_TESTS)
    add_subdirectory(tests)
endif ()