#![cfg(test)]
//!
//! Common test utilities shared across the engine_client crate.
//! Note: This module is only compiled during test builds (`#[cfg(test)]`).
//!

use crate::platform::WindowHandle;
use std::sync::Weak; // Use crate::platform here

// === Error Helpers ===
pub mod errors {
    use std::error::Error;
    use std::fmt;

    #[derive(Debug)]
    pub enum TestError {
        Initialization(String),
        Operation(String),
        InvalidState(String),
    }

    impl fmt::Display for TestError {
        fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
            match self {
                TestError::Initialization(msg) => write!(f, "Initialization error: {}", msg),
                TestError::Operation(msg) => write!(f, "Operation error: {}", msg),
                TestError::InvalidState(msg) => write!(f, "Invalid state: {}", msg),
            }
        }
    }

    impl Error for TestError {}

    pub type TestResult<T> = Result<T, TestError>;

    // Allow converting String/&str directly to TestError::Operation
    impl From<String> for TestError {
        fn from(msg: String) -> Self {
            TestError::Operation(msg)
        }
    }

    impl From<&str> for TestError {
        fn from(msg: &str) -> Self {
            TestError::Operation(msg.to_string())
        }
    }

    #[test]
    fn test_error_creation_and_conversion() {
        let err_init = TestError::Initialization("init failed".into());
        assert!(err_init.to_string().contains("Initialization error"));

        let err_op_str: TestError = "operation failed".into();
        assert!(matches!(err_op_str, TestError::Operation(_)));
        assert!(err_op_str.to_string().contains("Operation error"));

        let err_op_string: TestError = String::from("operation failed string").into();
        assert!(matches!(err_op_string, TestError::Operation(_)));
    }
}

// === Window Helpers ===
pub mod window {
    use super::*; // To get WindowHandle

    /// Create a test window handle (NOTE: This handle will NOT upgrade successfully).
    /// Used when a non-dangling handle *type* is needed, but liveness isn't tested.
    pub fn create_test_window_handle() -> WindowHandle {
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

    #[test]
    fn test_window_handles() {
        let invalid_handle = create_invalid_window_handle();
        assert!(
            invalid_handle.window.upgrade().is_none(),
            "Invalid handle should not be upgradeable"
        );

        let test_handle = create_test_window_handle();
        assert!(
            test_handle.window.upgrade().is_none(),
            "Test handle (as implemented) should not be upgradeable"
        );
    }
}

// === Vulkan Test Helpers (Moved from src/renderer/backends/vulkan/tests/helpers.rs) ===
// Keep these separate if they are *only* for Vulkan backend tests,
// or merge if generally useful. Let's keep them here for now.
pub mod vulkan_helpers {
    // Note: This needs access to VulkanContext, which might be private.
    // If VulkanContext is in super::super::, this needs adjustment.
    // Assuming VulkanContext is accessible via crate path for now.
    use crate::renderer::backends::vulkan::VulkanContext; // Adjust path if needed
    use std::sync::atomic::{AtomicBool, Ordering};

    static VULKAN_AVAILABLE: AtomicBool = AtomicBool::new(true);
    static DEVICE_DESTROYED: AtomicBool = AtomicBool::new(false);
    static DEVICE_CREATED: AtomicBool = AtomicBool::new(false);
    // Add swapchain tracking if needed later
    // static SWAPCHAIN_DESTROYED: AtomicBool = AtomicBool::new(false);

    pub fn reset_vulkan_state() {
        VULKAN_AVAILABLE.store(true, Ordering::SeqCst);
        DEVICE_DESTROYED.store(false, Ordering::SeqCst);
        DEVICE_CREATED.store(false, Ordering::SeqCst);
        // SWAPCHAIN_DESTROYED.store(false, Ordering::SeqCst);
    }

    pub fn is_vulkan_available() -> bool {
        VULKAN_AVAILABLE.load(Ordering::SeqCst)
    }
    pub fn set_vulkan_available(available: bool) {
        VULKAN_AVAILABLE.store(available, Ordering::SeqCst)
    }
    pub fn is_device_created() -> bool {
        DEVICE_CREATED.load(Ordering::SeqCst)
    }
    pub fn is_device_destroyed() -> bool {
        DEVICE_DESTROYED.load(Ordering::SeqCst)
    }
    pub fn mark_device_created() {
        DEVICE_CREATED.store(true, Ordering::SeqCst);
    }
    pub fn mark_device_destroyed() {
        DEVICE_DESTROYED.store(true, Ordering::SeqCst);
    }
    // pub fn is_swapchain_destroyed() -> bool { SWAPCHAIN_DESTROYED.load(Ordering::SeqCst) }
    // pub fn mark_swapchain_destroyed() { SWAPCHAIN_DESTROYED.store(true, Ordering::SeqCst); }

    /// Create a mock Vulkan context for testing
    /// TODO: Implement this properly if needed, requires mocking ash types.
    pub fn create_mock_context() -> VulkanContext {
        panic!("Mock Vulkan context creation is not implemented yet!");
        // Requires creating mock versions of ash::Instance, ash::Device, etc.
        // Or using a library like `mockall` extensively.
    }

    // Add the test from the original helpers file
    #[test]
    fn test_vulkan_state_tracking() {
        reset_vulkan_state();
        assert!(is_vulkan_available());
        assert!(!is_device_created());
        assert!(!is_device_destroyed());

        set_vulkan_available(false);
        assert!(!is_vulkan_available());

        mark_device_created();
        assert!(is_device_created());

        mark_device_destroyed();
        assert!(is_device_destroyed());
    }
}
