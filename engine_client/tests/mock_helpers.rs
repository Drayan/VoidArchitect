//!
//! Safe mock utilities for testing
//!

use std::ptr::NonNull;
use void_architect_engine_client::platform::WindowHandle;
use winit::window::Window;

/// Creates a safer mock window handle for testing
///
/// This is meant to be used in tests to avoid unsafe transmute operations
/// that could cause undefined behavior.
pub fn create_mock_window_handle() -> WindowHandle<'static> {
    // Create a dangling non-null pointer - this is still unsafe but safer than transmuting 0
    // We're doing this purely for testing and the Handle will not be dereferenced
    let window_ptr = NonNull::<Window>::dangling();

    WindowHandle {
        // SAFETY: This is for testing only. The window reference is never dereferenced.
        // All operations that would use the window check for null/dangling and return errors.
        window: unsafe { &*(window_ptr.as_ptr()) },
    }
}
