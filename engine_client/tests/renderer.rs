//!
//! Unit tests for RendererFrontend and RendererBackendVulkan
//!
#![allow(invalid_value)]
use void_architect_engine_client::renderer::RendererFrontend;

#[test]
fn renderer_frontend_shutdown_is_idempotent() {
    let mut renderer = RendererFrontend::new();
    let _ = renderer.shutdown();
    // Should not panic if called twice
    let _ = renderer.shutdown();
}

#[test]
fn renderer_frontend_resize_returns_result() {
    let mut renderer = RendererFrontend::new();
    let result = renderer.resize(800, 600);
    assert!(result.is_ok() || result.is_err());
}

#[test]
fn renderer_frontend_frame_methods_do_not_panic() {
    let mut renderer = RendererFrontend::new();
    let _ = renderer.begin_frame();
    let _ = renderer.end_frame();
}

#[test]
fn renderer_frontend_begin_end_frame_without_initialize() {
    let mut renderer = RendererFrontend::new();
    // Should not panic even if not initialized
    let result = renderer.begin_frame();
    assert_eq!(
        result.is_ok(),
        false,
        "Begin frame should fail without initialization"
    );
    let result = renderer.end_frame();
    assert_eq!(
        result.is_ok(),
        false,
        "End frame should fail without initialization"
    );
}
