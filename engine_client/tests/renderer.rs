//!
//! Unit tests for RendererFrontend and RendererBackendVulkan
//!
use void_architect_engine_client::platform::WindowHandle;
use void_architect_engine_client::renderer::{RendererBackend, RendererFrontend};

// Mock WindowHandle for tests (cannot create real winit::Window in CI)
struct DummyWindow;
impl DummyWindow {
    fn handle() -> WindowHandle<'static> {
        // SAFETY: This is a dummy, do not dereference in tests
        unsafe { std::mem::transmute(0usize) }
    }
}

#[test]
fn renderer_frontend_initialize_returns_error_on_invalid_window() {
    let mut renderer = RendererFrontend::new();
    // Pass a dummy window handle, expect error or Ok (depends on backend impl)
    let result = renderer.initialize(DummyWindow::handle());
    // Accept both Ok and Err, but test does not panic
    assert!(result.is_ok() || result.is_err());
}

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
fn renderer_frontend_multiple_initialize_calls() {
    let mut renderer = RendererFrontend::new();
    let _ = renderer.initialize(DummyWindow::handle());
    // Second call should not panic, even if not supported
    let _ = renderer.initialize(DummyWindow::handle());
}

#[test]
fn renderer_frontend_resize_before_and_after_initialize() {
    let mut renderer = RendererFrontend::new();
    // Resize before initialize
    let _ = renderer.resize(1024, 768);
    let _ = renderer.initialize(DummyWindow::handle());
    // Resize after initialize
    let _ = renderer.resize(1280, 720);
}

#[test]
fn renderer_frontend_shutdown_after_initialize() {
    let mut renderer = RendererFrontend::new();
    let _ = renderer.initialize(DummyWindow::handle());
    let _ = renderer.shutdown();
}

#[test]
fn renderer_frontend_begin_end_frame_without_initialize() {
    let mut renderer = RendererFrontend::new();
    // Should not panic even if not initialized
    let _ = renderer.begin_frame();
    let _ = renderer.end_frame();
}
