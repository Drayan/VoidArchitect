project(VoidArchitect_Tests VERSION 0.1.0 LANGUAGES CXX)

# Enable testing support
enable_testing()

# List test source files
file(GLOB_RECURSE TEST_SOURCES "src/*.cpp")
file(GLOB_RECURSE TEST_HEADERS "src/*.hpp")

# Create test executable (client of VoidArchitect_Engine)
add_executable(VoidArchitect_Tests
        ${TEST_SOURCES}
        ${TEST_HEADERS}
)

# Link with the engine library
target_link_libraries(VoidArchitect_Tests PRIVATE VoidArchitect_Engine)

# Use precompiled headers from engine
target_precompile_headers(VoidArchitect_Tests REUSE_FROM VoidArchitect_Engine)

# Platform-specific configuration
if (APPLE)
    # macOS settings
    target_compile_definitions(VoidArchitect_Tests PRIVATE "VOID_ARCH_MACOS")
    set_target_properties(VoidArchitect_Tests PROPERTIES
            INSTALL_RPATH "@executable_path/../../lib/${CMAKE_SYSTEM_NAME}"
            BUILD_WITH_INSTALL_RPATH TRUE
    )
elseif (WIN32)
    # Windows settings
    target_compile_definitions(VoidArchitect_Tests PRIVATE "VOID_ARCH_WINDOWS")
elseif (UNIX AND NOT APPLE)
    # Linux settings
    target_compile_definitions(VoidArchitect_Tests PRIVATE "VOID_ARCH_LINUX")
    set_target_properties(VoidArchitect_Tests PROPERTIES
            INSTALL_RPATH "$ORIGIN/../../lib/${CMAKE_SYSTEM_NAME}"
            BUILD_WITH_INSTALL_RPATH TRUE
    )
endif ()

# Include directories
target_include_directories(VoidArchitect_Tests
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Set C++ standard (same as engine)
target_compile_features(VoidArchitect_Tests PUBLIC cxx_std_20)

# Add testing-specific definitions
target_compile_definitions(VoidArchitect_Tests PRIVATE
        VA_TESTING_ENABLED=1
        VA_DEBUG_TESTS=1
)

# Compiler warnings and options
if (MSVC)
    target_compile_options(VoidArchitect_Tests PRIVATE /W4)
else ()
    target_compile_options(VoidArchitect_Tests PRIVATE -Wall -Wextra -Wpedantic)
endif ()

set_target_properties(VoidArchitect_Tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/${CMAKE_SYSTEM_NAME}/bin
)

# Test discovery and registration with CTest
set(TEST_CATEGORIES
        Collections
        JobSystem
        # Add more categoreis as needed
)

# Register individual test categories with CTest
foreach (CATEGORY ${TEST_CATEGORIES})
    add_test(
            NAME "VoidArchitect_${CATEGORY}_Tests"
            COMMAND VoidArchitect_Tests
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    # Set test properties
    set_tests_properties("VoidArchitect_${CATEGORY}_Tests" PROPERTIES
            TIMEOUT 60
            LABELS "${CATEGORY}"
    )
endforeach ()

# Register full test suite
add_test(
        NAME "VoidArchitect_AllTests"
        COMMAND VoidArchitect_Tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties("VoidArchitect_AllTests" PROPERTIES
        TIMEOUT 300
        LABELS "FullSuite"
)

# Custom targets for convenience
add_custom_target(run_tests
        COMMAND VoidArchitect_Tests
        DEPENDS VoidArchitect_Tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Running VoidArchitect tests with verbose output"
)

# Install test executable (optional, for packaging)
if (VA_INSTALL_TESTS)
    install(TARGETS VoidArchitect_Tests
            RUNTIME DESTINATION bin
            COMPONENT Testing
    )
endif ()

# Enable code coverage if requested
if (VA_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(VoidArchitect_Tests PRIVATE
            --coverage
            -fprofile-arcs
            -ftest-coverage
    )
    target_link_options(VoidArchitect_Tests PRIVATE
            --coverage
            -fprofile-arcs
            -ftest-coverage
    )
endif ()

# Print configuration summary
message(STATUS "VoidArchitect Tests Configuration:")
message(STATUS "  Test executable: VoidArchitect_Tests")
message(STATUS "  Test categories: ${TEST_CATEGORIES}")
message(STATUS "  Use 'make test' or 'ctest' to run via CTest")
message(STATUS "  Use 'make run_tests' for direct execution")
message(STATUS "  Use 'make list_tests' to see available tests")