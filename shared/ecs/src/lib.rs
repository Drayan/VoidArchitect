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

// This re-exports the derive macro for the Component trait.
// Users can then do `#[derive(Component)]` on their structs.
pub use void_architect_ecs_macros::Component as ComponentMacro;
// Note: The line above was `pub use void_architect_ecs_macros::Component;`
// If the macro is also named `Component`, it can conflict with the trait `Component`.
// It's common practice to rename the derive macro on import if the trait has the same name,
// e.g., `pub use some_ecs_macros::Component as DeriveComponent;`
// Or, ensure the macro generates code that uses `crate::component::Component`.
// For now, I've aliased it to `ComponentMacro` to avoid a direct name clash
// with the `Component` trait itself if both are brought into scope with `use crate::ecs::*;`.
// If the macro is designed to be used as `#[derive(ecs::Component)]` and the trait as `ecs::Component`,
// the original `pub use void_architect_ecs_macros::Component;` might be fine,
// but aliasing is safer to prevent ambiguity for users of the crate.
// Let's revert to the original if it's known to be non-conflicting or handled by path qualification.
// Reverting to original for now, assuming the macro and trait are used with paths or are distinct enough.
// pub use void_architect_ecs_macros::Component; // Original line

// Let's stick to the original provided line, assuming it's handled.
pub use void_architect_ecs_macros::Component;
