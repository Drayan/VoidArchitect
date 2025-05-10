pub mod archetype;
pub mod component;
pub mod entity;
pub mod registry;

// Core ECS Traits and Types
pub use component::{Component, ComponentTypeId};
pub use entity::Entity;
pub use registry::Registry;

// Potentially useful for advanced interaction or debugging, but less common for typical users.
// Consider if these should be pub or pub(crate) based on intended API surface.
pub use archetype::ArchetypeId;

// --- Macros ---
pub use void_architect_ecs_macros::Component;
