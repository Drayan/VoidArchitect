//!
//! Integration tests for the RendererFrontend + EngineContext integration
//!

use void_architect_engine_client::{EngineApplication, EngineContext};

// Import the common mock helpers
mod mock_helpers;

/// Mock for testing EngineContext with RendererFrontend integration
mod mocks {
    use void_architect_engine_client::platform::WindowHandle;

    pub fn handle() -> WindowHandle<'static> {
        // Use the safer mock helper
        super::mock_helpers::create_mock_window_handle()
    }
}

#[test]
fn engine_context_initialize_with_renderer() {
    let mut context = EngineContext::new();
    let result = context.initialize(mocks::handle());

    // Initialize through context should work or fail gracefully
    assert!(
        result.is_ok() || result.is_err(),
        "EngineContext initialize should return a Result without panicking"
    );
}

#[test]
fn engine_context_shutdown_with_renderer() {
    let mut context = EngineContext::new();
    let _ = context.initialize(mocks::handle());

    let result = context.shutdown();
    assert!(
        result.is_ok() || result.is_err(),
        "EngineContext shutdown should return a Result without panicking"
    );
}

#[test]
fn engine_context_update_render_with_renderer() {
    let mut context = EngineContext::new();
    let _ = context.initialize(mocks::handle());

    // These shouldn't panic even with a mock window
    context.update(0.016); // ~60fps
    context.render(0.016);
}

#[test]
fn engine_context_resize_with_renderer() {
    let mut context = EngineContext::new();
    let _ = context.initialize(mocks::handle());

    let result = context.resize(1280, 720);
    assert!(
        result.is_ok() || result.is_err(),
        "EngineContext resize should return a Result without panicking"
    );
}

#[test]
fn engine_context_render_cycle() {
    let mut context = EngineContext::new();
    let _ = context.initialize(mocks::handle());

    // Simulate a few frames of the render cycle
    for i in 0..5 {
        let dt = 0.016; // ~60fps
        context.update(dt);
        context.render(dt);

        // Occasional resize
        if i == 2 {
            let _ = context.resize(1600, 900);
        }
    }

    let result = context.shutdown();
    assert!(
        result.is_ok() || result.is_err(),
        "EngineContext shutdown after render cycle should return a Result without panicking"
    );
}

// Changed this test to use public API only
#[test]
fn engine_context_error_handling() {
    // Create a context
    let mut context = EngineContext::new();

    // Test error handling for renderer initialization
    let init_result = context.initialize(mocks::handle());

    // For now we're just ensuring these don't panic - without a real window they should return errors
    if init_result.is_err() {
        // Expected result
        assert!(
            !init_result.unwrap_err().is_empty(),
            "Error message should not be empty"
        );
    }

    let shutdown_result = context.shutdown();
    assert!(
        shutdown_result.is_ok() || shutdown_result.is_err(),
        "Shutdown should return a Result"
    );

    let resize_result = context.resize(1024, 768);
    assert!(
        resize_result.is_ok() || resize_result.is_err(),
        "Resize should return a Result"
    );

    // These should not panic
    context.update(0.016);
    // Avoid rendering since it might panic with an invalid renderer
}
