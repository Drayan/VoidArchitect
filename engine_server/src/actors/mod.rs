//! # Actors Module
//!
//! This module contains the Actix actors responsible for handling various
//! aspects of the server logic, such as network connections, session management,
//! game state, and persistence.

pub mod handshake;
pub mod network_listener;
pub mod session_manager;
pub use session_manager::{SessionManagerActor, UnregisterSession}; // Ensure RegisterSession is not exported

// TODO: Add session module once SessionActor is created
// pub mod session;
// pub use session::SessionActor;

// Add other actor modules here as they are created (e.g., universe, persistence, etc.)
