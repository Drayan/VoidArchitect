// Integration tests for the SDL3 PlatformLayer.
// NOTE: SDL initialization might require specific environment setup or main thread execution,
// which might not work reliably in all test environments, especially on macOS.
// These tests focus on the state management and API of PlatformLayer.

// Import the necessary items from the engine_client crate.
// Assuming platform_sdl.rs will be renamed to platform.rs and exposed at crate::platform

const TEST_TITLE: &str = "Test Window";
const TEST_WIDTH: u32 = 800;
const TEST_HEIGHT: u32 = 600;

// Helper to initialize platform layer for tests
// We wrap this to potentially skip on macOS if needed, although SDL3 might be more flexible.
#[cfg(not(target_os = "macos"))]
fn initialize_test_platform() -> PlatformLayer {
    let mut platform = PlatformLayer::new();
    // Use assert! or expect for test setup, as panics indicate setup failure.
    platform
        .initialize(TEST_TITLE, TEST_WIDTH, TEST_HEIGHT)
        .expect("PlatformLayer failed to initialize for test");
    platform
}

#[cfg(not(target_os = "macos"))]
#[test]
fn test_platform_layer_new() {
    let platform = PlatformLayer::new();
    // Check initial state (internal fields are private, check observable behavior)
    assert!(
        platform.get_window_handle().is_none(),
        "Window handle should be None initially"
    );
    // Check default size (assuming getters work before init, or test after init)
    // Let's test size getters after initialization instead for robustness.
}

#[cfg(not(target_os = "macos"))]
#[test]
fn test_platform_layer_initialize_success() {
    let mut platform = PlatformLayer::new();
    let result = platform.initialize(TEST_TITLE, TEST_WIDTH, TEST_HEIGHT);

    assert!(result.is_ok(), "Initialization should succeed");
    assert!(
        platform.get_window_handle().is_some(),
        "Window handle should be Some after init"
    );

    // Check if the weak handle can be upgraded (indicates window Arc exists)
    let handle = platform.get_window_handle().unwrap();
    assert!(
        handle.window.upgrade().is_some(),
        "Weak window handle should be upgradable"
    );

    // Check if size getters return the initialized values
    assert_eq!(
        platform.get_window_width(),
        TEST_WIDTH,
        "Width should match initial value"
    );
    assert_eq!(
        platform.get_window_height(),
        TEST_HEIGHT,
        "Height should match initial value"
    );

    // Title is internal, cannot directly check here unless a getter is added.

    // Ensure shutdown cleans up
    platform.shutdown();
}

#[cfg(not(target_os = "macos"))]
#[test]
fn test_platform_layer_run_before_initialize() {
    let mut platform = PlatformLayer::new();
    let result = platform.run();

    assert!(result.is_err(), "run() before initialize should return Err");
    match result.err().unwrap() {
        PlatformError::NotInitialized(_) => {} // Expected error variant
        e => panic!("Expected PlatformError::NotInitialized, got {:?}", e),
    }
}

#[cfg(not(target_os = "macos"))]
#[test]
fn test_platform_layer_shutdown() {
    let mut platform = initialize_test_platform();

    // Pre-shutdown checks
    assert!(
        platform.get_window_handle().is_some(),
        "Window handle should be Some before shutdown"
    );
    let handle_before = platform.get_window_handle().unwrap();
    let arc_before = handle_before.window.upgrade();
    assert!(
        arc_before.is_some(),
        "Window Arc should exist before shutdown"
    );

    platform.shutdown();

    // Post-shutdown checks
    assert!(
        platform.get_window_handle().is_none(),
        "Window handle should be None after shutdown"
    );

    // Check if the previously obtained weak handle can no longer be upgraded
    assert!(
        handle_before.window.upgrade().is_none(),
        "Weak handle should not be upgradable after shutdown"
    );
    // Check if the Arc was indeed dropped (optional, harder to check directly without access)
    // drop(arc_before); // Drop our reference
    // We assume the PlatformLayer dropped its Arc in shutdown.
}

#[cfg(not(target_os = "macos"))]
#[test]
fn test_platform_layer_get_window_handle() {
    let mut platform = PlatformLayer::new();
    assert!(
        platform.get_window_handle().is_none(),
        "Handle should be None before init"
    );

    platform.initialize(TEST_TITLE, TEST_WIDTH, TEST_HEIGHT).unwrap();
    let handle = platform.get_window_handle();
    assert!(handle.is_some(), "Handle should be Some after init");
    assert!(
        handle.unwrap().window.upgrade().is_some(),
        "Weak handle should upgrade after init"
    );

    platform.shutdown();
    assert!(
        platform.get_window_handle().is_none(),
        "Handle should be None after shutdown"
    );
}

#[cfg(not(target_os = "macos"))]
#[test]
fn test_platform_layer_get_window_size() {
    // Test initial (default) sizes might be misleading if they aren't guaranteed before init.
    // Test after initialization.
    let mut platform = initialize_test_platform();
    assert_eq!(
        platform.get_window_width(),
        TEST_WIDTH,
        "Width should match initialized width"
    );
    assert_eq!(
        platform.get_window_height(),
        TEST_HEIGHT,
        "Height should match initialized height"
    );

    // Note: Testing size *after* a resize event requires running the event loop,
    // which is complex for unit tests. We trust the `run` method updates internal state correctly.
    platform.shutdown();
}

// TODO: Add tests for error conditions during initialization if possible (might require mocking SDL).
// TODO: Add tests for event handling logic if an event propagation mechanism is added.
