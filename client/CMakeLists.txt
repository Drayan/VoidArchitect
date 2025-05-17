project(VoidArchitect_Client VERSION 0.1.0 LANGUAGES CXX)

# List source files
file(GLOB_RECURSE CLIENT_SOURCES "src/*.cpp")
file(GLOB_RECURSE CLIENT_HEADERS "src/*.hpp")

# Create the executable
add_executable(VoidArchitect_Client
        ${CLIENT_SOURCES}
        ${CLIENT_HEADERS}
)

# Link with the engine library
target_link_libraries(VoidArchitect_Client PRIVATE VoidArchitect_EngineClient)

target_precompile_headers(VoidArchitect_Client REUSE_FROM VoidArchitect_EngineClient)
# Platform-specific configuration
if (APPLE)
    # macOS settings
    target_compile_definitions(VoidArchitect_Client PRIVATE "VOID_ARCH_MACOS")
    set_target_properties(VoidArchitect_Client PROPERTIES
            INSTALL_RPATH "@executable_path/../../lib/${CMAKE_SYSTEM_NAME}"
            BUILD_WITH_INSTALL_RPATH TRUE
    )
elseif (WIN32)
    # Windows settings
    target_compile_definitions(VoidArchitect_Client PRIVATE "VOID_ARCH_WINDOWS")
elseif (UNIX AND NOT APPLE)
    # Linux settings
    target_compile_definitions(VoidArchitect_Client PRIVATE "VOID_ARCH_LINUX")
    set_target_properties(VoidArchitect_Client PROPERTIES
            INSTALL_RPATH "$ORIGIN/../../lib/${CMAKE_SYSTEM_NAME}"
            BUILD_WITH_INSTALL_RPATH TRUE
    )
endif ()

# Include directories
target_include_directories(VoidArchitect_Client
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Installation
install(TARGETS VoidArchitect_Client
        RUNTIME DESTINATION bin
)

# Tests configuration
if (VOID_ARCHITECT_BUILD_TESTS)
    add_subdirectory(tests)
endif ()