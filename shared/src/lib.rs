//! Shared library for protocol messages and types used by both client and server.
//!
//! This crate exposes the generated Rust code from Protobuf definitions, as well as any
//! shared structs, enums, and traits required for network communication and state sync.

pub mod messages {
    include!(concat!(env!("OUT_DIR"), "/messages.rs"));
}
