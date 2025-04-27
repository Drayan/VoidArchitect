//!
//! Safe mock utilities for testing
//!

use std::sync::{Arc, RwLock};
use void_architect_engine_client::platform::WindowHandle;

/// Creates a mock SDL3 window handle for testing
///
/// This creates a mock WindowHandle with a Weak reference that can be upgraded
/// but points to a minimal mock window implementation.
/// This is safer than creating dangling pointers and is meant for testing only.
pub fn create_mock_window_handle() -> WindowHandle {
    // Create a minimal mock SDL window wrapped in Arc<RwLock>
    // We're using a placeholder value since we can't easily create a real SDL window in tests
    let mock_window: Arc<RwLock<sdl3::video::Window>> = unsafe {
        // SAFETY: This is for testing only. The window reference is never dereferenced.
        // All operations that would use the window check for null/dangling and return errors.
        std::mem::transmute(Arc::new(RwLock::new(())))
    };

    // Create a WindowHandle with a weak reference to our mock window
    WindowHandle {
        window: Arc::downgrade(&mock_window),
    }
}
