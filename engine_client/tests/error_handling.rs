//!
//! Integration tests for error handling
//!
mod helpers;
use helpers::TestResult;
use helpers::window::create_invalid_window_handle;
use void_architect_engine_client::renderer::RendererFrontend;

#[test]
fn test_invalid_window_initialization() -> TestResult<()> {
    let mut renderer = RendererFrontend::new();
    let invalid_handle = create_invalid_window_handle();

    match renderer.initialize(invalid_handle) {
        Ok(_) => Err("Expected initialization to fail with invalid handle".into()),
        Err(err) => {
            assert!(!err.is_empty(), "Error message should not be empty");
            Ok(())
        }
    }
}

#[test]
fn test_uninitialized_operations() -> TestResult<()> {
    let mut renderer = RendererFrontend::new();

    // Test frame operations before initialization
    assert!(renderer.begin_frame().is_err(), "Begin frame should fail");
    assert!(renderer.end_frame().is_err(), "End frame should fail");
    assert!(renderer.resize(800, 600).is_err(), "Resize should fail");

    Ok(())
}

// #[test]
// fn test_resize_errors() -> TestResult<()> {
//     let mut renderer = RendererFrontend::new();
//     let handle = create_test_window_handle();

//     renderer.initialize(handle)?;

//     // Invalid dimensions
//     assert!(
//         renderer.resize(0, 0).is_err(),
//         "Resize with zero dimensions should fail"
//     );

//     // Begin a frame and try to resize
//     renderer.begin_frame()?;
//     assert!(
//         renderer.resize(1024, 768).is_err(),
//         "Resize during frame should fail"
//     );
//     renderer.end_frame()?;

//     Ok(())
// }

// #[test]
// fn test_frame_nesting_errors() -> TestResult<()> {
//     let mut renderer = RendererFrontend::new();
//     let handle = create_test_window_handle();

//     renderer.initialize(handle)?;

//     // Nested begin_frame
//     renderer.begin_frame()?;
//     assert!(
//         renderer.begin_frame().is_err(),
//         "Nested begin_frame should fail"
//     );
//     renderer.end_frame()?;

//     // end_frame without begin
//     assert!(
//         renderer.end_frame().is_err(),
//         "end_frame without begin should fail"
//     );

//     Ok(())
// }

// #[test]
// fn test_shutdown_errors() -> TestResult<()> {
//     let mut renderer = RendererFrontend::new();

//     // Shutdown without initialization
//     assert!(
//         renderer.shutdown().is_err(),
//         "Shutdown without initialization should fail"
//     );

//     // Initialize and shutdown
//     let handle = create_test_window_handle();
//     renderer.initialize(handle)?;
//     renderer.shutdown()?;

//     // Double shutdown
//     assert!(renderer.shutdown().is_err(), "Second shutdown should fail");

//     Ok(())
// }
