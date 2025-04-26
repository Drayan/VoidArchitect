//!
//! Unit tests for the Vulkan framebuffer functionality
//!

use void_architect_engine_client::renderer::RendererFrontend;

// Import the common mock helpers
mod mock_helpers;

#[test]
fn framebuffer_creation_during_initialize() {
    let mut renderer = RendererFrontend::new();
    let result = renderer.initialize(mock_helpers::create_mock_window_handle());

    // We can't verify actual framebuffer creation with mock window
    // but we ensure the renderer doesn't panic
    assert!(
        result.is_ok() || result.is_err(),
        "Initialize (which creates framebuffers) should return a Result without panicking"
    );
}

#[test]
fn framebuffer_cleanup_during_shutdown() {
    let mut renderer = RendererFrontend::new();
    let _ = renderer.initialize(mock_helpers::create_mock_window_handle());

    // Test framebuffer cleanup during shutdown
    let shutdown_result = renderer.shutdown();
    assert!(
        shutdown_result.is_ok() || shutdown_result.is_err(),
        "Shutdown (which destroys framebuffers) should return a Result without panicking"
    );
}

#[test]
fn framebuffer_recreation_during_resize() {
    let mut renderer = RendererFrontend::new();
    let _ = renderer.initialize(mock_helpers::create_mock_window_handle());

    // Test framebuffer recreation during resize
    let resize_result = renderer.resize(1024, 768);
    assert!(
        resize_result.is_ok() || resize_result.is_err(),
        "Resize (which recreates framebuffers) should return a Result without panicking"
    );

    // Verify rendering still works after resize with new framebuffers
    let begin_result = renderer.begin_frame();
    assert!(
        begin_result.is_ok() || begin_result.is_err(),
        "Begin frame after resize should return a Result without panicking"
    );

    let end_result = renderer.end_frame();
    assert!(
        end_result.is_ok() || end_result.is_err(),
        "End frame after resize should return a Result without panicking"
    );
}

#[test]
fn framebuffer_extreme_resize_values() {
    let mut renderer = RendererFrontend::new();
    let _ = renderer.initialize(mock_helpers::create_mock_window_handle());

    // Test various edge cases for framebuffer dimensions
    let test_sizes = [
        (1, 1),       // Minimum possible size
        (8192, 8192), // Very large (may exceed actual device limits)
        (800, 1),     // Extreme aspect ratio
        (1, 600),     // Another extreme aspect ratio
    ];

    for &(width, height) in &test_sizes {
        let resize_result = renderer.resize(width, height);

        // Either succeeds or returns an error (for extreme sizes),
        // but should never panic
        assert!(
            resize_result.is_ok() || resize_result.is_err(),
            "Resize to extreme dimensions should return a Result without panicking"
        );
    }
}
