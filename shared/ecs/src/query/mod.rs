//! ECS query mechanism.
//!
//! This module provides the core logic for querying entities based on the components
//! they possess. It includes:
//! - `QueryParam`: A trait to define what can be queried.
//! - `Fetch`: A trait for low-level data retrieval.
//! - `QueryIter`: The main iterator for executing queries.
//! - `AccessType`, `ComponentAccessInfo`, `QueryDescriptor`: Structs for describing
//!   and managing component access within queries.

pub mod access;
pub mod fetch;
pub mod iter;
pub mod registry_query; // Adjusted name

pub use access::{AccessType, ComponentAccessInfo, QueryDescriptor, QueryValidationError}; // Added QueryValidationError
pub use fetch::Fetch;
pub use iter::QueryIter;
pub use registry_query::QueryParam;

// TODO: Consider adding a prelude for common query-related imports if needed.
// pub mod prelude {
//     pub use super::QueryParam;
//     pub use super::QueryIter;
//     // ... other common items
// }
