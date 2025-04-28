//!
//! Mock window implementation for tests
//!

use std::sync::Weak;
use void_architect_engine_client::platform::WindowHandle;
// Note: We don't import sdl3::video::Window as we only use Weak::new()

/// Create a test window handle (NOTE: This handle will NOT upgrade successfully).
/// Used when a non-dangling handle *type* is needed, but liveness isn't tested.
pub fn create_test_window_handle() -> WindowHandle {
    // Returns a dangling handle, same as invalid for upgrade checks.
    // Tests needing a live handle require a different setup (e.g., full integration test).
    WindowHandle {
        window: Weak::new(),
    }
}

/// Create an invalid window handle (dangling weak reference).
pub fn create_invalid_window_handle() -> WindowHandle {
    WindowHandle {
        window: Weak::new(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_window_handles() {
        // Test the invalid handle creation
        let invalid_handle = create_invalid_window_handle();
        assert!(
            invalid_handle.window.upgrade().is_none(),
            "Invalid handle should not be upgradeable"
        );

        // Test the "test" handle creation (which is also dangling in this implementation)
        let test_handle = create_test_window_handle();
        assert!(
            test_handle.window.upgrade().is_none(),
            "Test handle (as implemented) should not be upgradeable"
        );
        // If tests elsewhere *require* an upgradeable handle, those tests might fail
        // and need adjustment or a more sophisticated mocking strategy if this helper is used.
    }
}
