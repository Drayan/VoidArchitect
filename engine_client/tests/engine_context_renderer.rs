mod helpers;
use helpers::TestResult;
use helpers::window::create_invalid_window_handle;
use void_architect_engine_client::{EngineContext, EngineError};

// #[test]
// fn test_engine_context_lifecycle() -> TestResult<()> {
//     let mut context = EngineContext::new();
//     let handle = create_window_handle(800, 600, "Test Window");

//     // Test initialization
//     context.initialize(handle)?;

//     // Test update and render
//     context.update(0.016)?;
//     context.render(0.016)?;

//     // Test resize
//     context.resize(1920, 1080)?;

//     // Test shutdown
//     context.shutdown()?;
//     Ok(())
// }

#[test]
fn test_engine_context_error_handling() -> TestResult<()> {
    let mut context = EngineContext::new();

    // Test with invalid window handle
    let invalid_handle = create_invalid_window_handle();
    assert!(
        context.initialize(invalid_handle).is_err(),
        "Initialize with invalid handle should fail"
    );

    // Test operations before initialization
    assert!(
        matches!(
            context.update(0.016),
            Err(EngineError::InitializationError(_))
        ),
        "Update before init should return InitializationError"
    );
    assert!(
        matches!(
            context.render(0.016),
            Err(EngineError::InitializationError(_))
        ),
        "Render before init should return InitializationError"
    );
    assert!(
        matches!(
            context.resize(1024, 768),
            Err(EngineError::InitializationError(_))
        ),
        "Resize before init should return InitializationError"
    );
    assert!(
        matches!(context.shutdown(), Err(EngineError::InitializationError(_))),
        "Shutdown before init should return InitializationError"
    );

    // Initialize with valid handle for further tests
    // let handle = cwindow_handle(800, 600, "Test Window");
    // context.initialize(handle)?;

    // // Test invalid resize
    // assert!(
    //     context.resize(0, 0).is_err(),
    //     "Resize with invalid dimensions should fail"
    // );
    Ok(())
}

// #[test]
// fn test_engine_context_render_cycle() -> TestResult<()> {
//     let mut context = EngineContext::new();
//     let handle = create_window_handle(800, 600, "Test Window");

//     context.initialize(handle)?;

//     // Run several frames
//     for i in 0..5 {
//         context.update(0.016)?;
//         context.render(0.016)?;

//         // Test resize in middle of render cycle
//         if i == 2 {
//             context.resize(1600, 900)?;
//         }
//     }

//     context.shutdown()?;
//     Ok(())
// }
