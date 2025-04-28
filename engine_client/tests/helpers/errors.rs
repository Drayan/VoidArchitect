//!
//! Error handling utilities for tests
//!

use std::error::Error;
use std::fmt;

#[derive(Debug)]
pub enum TestError {
    /// Error indicating a test initialization failure
    Initialization(String),
    /// Error indicating a test assertion or operation failure
    Operation(String),
    /// Error indicating an invalid test state
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

/// Type alias for Result with TestError
pub type TestResult<T> = Result<T, TestError>;

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

use void_architect_engine_client::EngineError;

impl From<EngineError> for TestError {
    fn from(err: EngineError) -> Self {
        match err {
            EngineError::InitializationError(msg) => TestError::Initialization(msg),
            _ => TestError::Operation(err.to_string()),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_error_creation() {
        let err = TestError::Initialization("test error".into());
        assert!(err.to_string().contains("Initialization error"));
    }

    #[test]
    fn test_error_conversion() {
        let err: TestError = "test error".into();
        assert!(matches!(err, TestError::Operation(_)));

        let err: TestError = String::from("test error").into();
        assert!(matches!(err, TestError::Operation(_)));
    }
}
