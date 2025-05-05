//!
//! Unit tests for get_config in server/src/main.rs
//!
//! This module contains tests for the get_config function, which reads server host and port configuration
//! from environment variables. It ensures correct fallback to defaults and proper reading of environment values.
// Import the function to test
use serial_test::serial;
use void_architect_engine_server::config::get_config;

fn with_env_vars<F: FnOnce()>(host: Option<String>, port: Option<String>, test: F) {
    let orig_host = ::std::env::var("VOID_SERVER_HOST").ok();
    let orig_port = ::std::env::var("VOID_SERVER_PORT").ok();
    match host {
        Some(val) => unsafe { ::std::env::set_var("VOID_SERVER_HOST", val) },
        None => unsafe { ::std::env::remove_var("VOID_SERVER_HOST") },
    }
    match port {
        Some(val) => unsafe { ::std::env::set_var("VOID_SERVER_PORT", val) },
        None => unsafe { ::std::env::remove_var("VOID_SERVER_PORT") },
    }
    // Run the test
    test();
    // Restore original values
    match orig_host {
        Some(val) => unsafe { ::std::env::set_var("VOID_SERVER_HOST", val) },
        None => unsafe { ::std::env::remove_var("VOID_SERVER_HOST") },
    }
    match orig_port {
        Some(val) => unsafe { ::std::env::set_var("VOID_SERVER_PORT", val) },
        None => unsafe { ::std::env::remove_var("VOID_SERVER_PORT") },
    }
}

#[test]
#[serial]
fn get_config_returns_defaults_when_env_absent() {
    with_env_vars(None, None, || {
        let (host, port) = get_config();
        assert_eq!(host, "127.0.0.1");
        assert_eq!(port, "4242");
    });
}

#[test]
#[serial]
fn get_config_reads_env_vars() {
    with_env_vars(
        Some("192.168.1.1".to_string()),
        Some("9999".to_string()),
        || {
            let (host, port) = get_config();
            assert_eq!(host, "192.168.1.1");
            assert_eq!(port, "9999");
        },
    );
}

#[test]
#[serial]
fn get_config_empty_env_vars_fallback_to_default() {
    with_env_vars(Some("".to_string()), Some("".to_string()), || {
        let (host, port) = get_config();
        assert_eq!(host, "127.0.0.1");
        assert_eq!(port, "4242");
    });
}
