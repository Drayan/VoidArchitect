project(VoidArchitect_Client VERSION 0.1.0 LANGUAGES CXX)

file(GLOB_RECURSE CLIENT_APP_SOURCES "src/*.cpp")
file(GLOB_RECURSE CLIENT_APP_HEADERS "src/*.hpp")

add_executable(VoidArchitect_Client
        ${CLIENT_APP_SOURCES}
        ${CLIENT_APP_HEADERS}
)

target_precompile_headers(VoidArchitect_Client REUSE_FROM VoidArchitectEngine_Client)

target_link_libraries(VoidArchitect_Client PRIVATE
        VoidArchitectEngine_Client
)

# Platform-specific configuration
if (APPLE)
    target_compile_definitions(VoidArchitect_Client PRIVATE "VOID_ARCH_MACOS")
    set_target_properties(VoidArchitect_Client PROPERTIES
            INSTALL_RPATH "@executable_path/../../lib/${CMAKE_SYSTEM_NAME}"
            BUILD_WITH_INSTALL_RPATH TRUE
    )
elseif (WIN32)
    target_compile_definitions(VoidArchitect_Client PRIVATE "VOID_ARCH_WINDOWS")
elseif (UNIX AND NOT APPLE)
    target_compile_definitions(VoidArchitect_Client PRIVATE "VOID_ARCH_LINUX")
    set_target_properties(VoidArchitect_Client PROPERTIES
            INSTALL_RPATH "$ORIGIN/../../lib/${CMAKE_SYSTEM_NAME}"
            BUILD_WITH_INSTALL_RPATH TRUE
    )
endif ()

target_include_directories(VoidArchitect_Client PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

install(TARGETS VoidArchitect_Client
        RUNTIME DESTINATION bin
)