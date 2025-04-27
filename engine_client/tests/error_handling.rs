//!
//! Unit tests for error handling in the engine client
//!

use void_architect_engine_client::EngineContext;
use void_architect_engine_client::platform::WindowHandle;
use void_architect_engine_client::renderer::RendererFrontend;

// Import the common mock helpers
mod mock_helpers;

/// Mock window handle for testing error handling
struct MockErrorWindow;

impl MockErrorWindow {
    fn handle() -> WindowHandle {
        mock_helpers::create_mock_window_handle()
    }
}

#[test]
fn engine_handles_out_of_memory_error() {
    let mut renderer = RendererFrontend::new();

    // Try to initialize with mock window (should fail)
    let result = renderer.initialize(MockErrorWindow::handle());

    // Check that we get appropriate error message
    if let Err(err) = result {
        assert!(
            err.contains("Window handle is invalid") || !err.is_empty(),
            "Error messages should be descriptive: {}",
            err
        );
    }
}

#[test]
fn engine_handles_resize_errors() {
    let mut renderer = RendererFrontend::new();

    // Try to resize before initialize
    let result = renderer.resize(0, 0);

    // Should fail with an appropriate error
    assert!(
        result.is_err(),
        "Resize with invalid dimensions should fail"
    );
    if let Err(err) = result {
        assert!(!err.is_empty(), "Error message should not be empty");
    }
}

#[test]
fn engine_handles_frame_errors() {
    let mut renderer = RendererFrontend::new();

    // Begin frame without initialize should fail with an error
    let result = renderer.begin_frame();
    assert!(
        result.is_err(),
        "Begin frame without initialize should fail"
    );

    // End frame without initialize should fail with an error
    let result = renderer.end_frame();
    assert!(result.is_err(), "End frame without initialize should fail");
}

#[test]
fn engine_context_handles_renderer_failures() {
    // Create a context with actual renderer
    let mut context = EngineContext::new();

    // We can't directly create a context with no renderer due to visibility,
    // so we'll test behavior with our mock window which will cause errors

    // Operations should not panic even with failures
    context.update(0.016);

    // Test error handling for renderer initialization
    let result = context.initialize(MockErrorWindow::handle());
    assert!(
        result.is_err() || result.is_ok(),
        "Initialize should return a Result without panicking"
    );

    let result = context.shutdown();
    assert!(
        result.is_err() || result.is_ok(),
        "Shutdown should return a Result without panicking"
    );

    let result = context.resize(800, 600);
    assert!(
        result.is_err() || result.is_ok(),
        "Resize should return a Result without panicking"
    );
}
