//! Server-side library for VoidArchitect.
//!
//! This module provides utility functions for the server, such as configuration loading from environment variables.

/// Reads the server host and port configuration from environment variables.
///
/// Returns a tuple `(host, port)` as strings. If the environment variables `VOID_SERVER_HOST` or
/// `VOID_SERVER_PORT` are not set or are empty, defaults to `127.0.0.1` and `4242` respectively.
pub fn get_config() -> (String, String) {
    let default_host = "127.0.0.1";
    let default_port = "4242";

    let host = match std::env::var("VOID_SERVER_HOST") {
        Ok(env_host) => env_host,
        Err(_) => default_host.to_string(),
    };
    let host = if host.is_empty() {
        default_host.to_string()
    } else {
        host
    };
    let port = match std::env::var("VOID_SERVER_PORT") {
        Ok(env_port) => env_port,
        Err(_) => default_port.to_string(),
    };
    let port = if port.is_empty() {
        default_port.to_string()
    } else {
        port
    };
    (host, port)
}
