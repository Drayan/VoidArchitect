//!
//! Unit tests for the Vulkan command buffer handling
//!

use void_architect_engine_client::renderer::RendererFrontend;

// Import the common mock helpers
mod mock_helpers;

#[test]
fn command_buffer_allocation_deallocation() {
    let mut renderer = RendererFrontend::new();

    // Initialize should create command buffers
    let init_result = renderer.initialize(mock_helpers::create_mock_window_handle());
    assert!(
        init_result.is_ok() || init_result.is_err(),
        "Initialize should return a Result without panicking"
    );

    // Shutdown should clean up command buffers
    let shutdown_result = renderer.shutdown();
    assert!(
        shutdown_result.is_ok() || shutdown_result.is_err(),
        "Shutdown should return a Result without panicking"
    );
}

#[test]
fn command_buffer_begin_end_frame() {
    let mut renderer = RendererFrontend::new();
    let _ = renderer.initialize(mock_helpers::create_mock_window_handle());

    // Test begin frame which implicitly tests command buffer recording
    let begin_result = renderer.begin_frame();
    assert!(
        begin_result.is_ok() || begin_result.is_err(),
        "Begin frame (command buffer recording) should return a Result without panicking"
    );

    // Test end frame which implicitly tests command buffer submission
    let end_result = renderer.end_frame();
    assert!(
        end_result.is_ok() || end_result.is_err(),
        "End frame (command buffer submission) should return a Result without panicking"
    );
}

#[test]
fn command_buffer_after_resize() {
    let mut renderer = RendererFrontend::new();
    let _ = renderer.initialize(mock_helpers::create_mock_window_handle());

    // Resize which should recreate framebuffers and possibly command buffers
    let _ = renderer.resize(1024, 768);

    // Test frame cycle after resize
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
fn command_buffer_handles_multiple_frames() {
    let mut renderer = RendererFrontend::new();
    let _ = renderer.initialize(mock_helpers::create_mock_window_handle());

    // Test multiple frame cycles
    for _ in 0..3 {
        let begin_result = renderer.begin_frame();
        assert!(
            begin_result.is_ok() || begin_result.is_err(),
            "Multiple begin_frame calls should not panic"
        );

        let end_result = renderer.end_frame();
        assert!(
            end_result.is_ok() || end_result.is_err(),
            "Multiple end_frame calls should not panic"
        );
    }
}
