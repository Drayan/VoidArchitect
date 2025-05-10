use super::access::{AccessType, ComponentAccessInfo};
use super::fetch::{Fetch, FetchEntity, FetchRead, FetchWrite}; // Added FetchEntity
use crate::component::{Component, ComponentTypeId};
use crate::entity::Entity;

// 'registry and 'state are lifetime parameters for the registry and query state
/// # Safety
/// Implementing this trait is unsafe because incorrect implementations can lead to
/// undefined behavior, primarily due to unsafe memory access patterns in the
/// associated `Fetch` implementations. Implementors must ensure that:
/// 1. `add_access` correctly reports all component accesses with the correct `AccessType`.
/// 2. The associated `Fetch` type correctly and safely accesses component data
///    according to the accesses declared by `add_access`.
/// 3. `Fetch::fetch` correctly uses the provided `Archetype` and `entity_index_in_archetype`
///    and respects the lifetimes `'registry` and `'state`.
pub unsafe trait QueryParam {
    // Using higher-ranked trait bounds (HRTB) to properly declare lifetimes
    type Item<'registry, 'state>
    where
        Self: 'registry; // The type returned by the iterator, e.g. (&'registry Position, &'registry mut Velocity)
    type Fetch<'registry, 'state>: Fetch<'registry, 'state, Self>
    where
        Self: 'registry;

    fn add_access(descriptor: &mut Vec<ComponentAccessInfo>);
    fn requests_entity_id() -> bool; // Does this part of the query request Entity ID?

    // TODO: Add validation logic here or in QueryDescriptor::new
    // fn validate_access(descriptor: &Vec<ComponentAccessInfo>) -> Result<(), QueryValidationError>;
}

unsafe impl<'registry_param, T: Component> QueryParam for &'registry_param T {
    type Item<'registry, 'state>
        = &'registry T
    where
        Self: 'registry;

    type Fetch<'registry, 'state>
        = FetchRead<T>
    where
        Self: 'registry;

    fn add_access(descriptor: &mut Vec<ComponentAccessInfo>) {
        descriptor.push(ComponentAccessInfo {
            component_id: ComponentTypeId::of::<T>(),
            access_type: AccessType::Read,
        });
    }

    fn requests_entity_id() -> bool {
        false
    }
}

unsafe impl<'registry_param, T: Component> QueryParam for &'registry_param mut T {
    type Item<'registry, 'state>
        = &'registry mut T
    where
        Self: 'registry;

    type Fetch<'registry, 'state>
        = FetchWrite<T>
    where
        Self: 'registry;

    fn add_access(descriptor: &mut Vec<ComponentAccessInfo>) {
        descriptor.push(ComponentAccessInfo {
            component_id: ComponentTypeId::of::<T>(),
            access_type: AccessType::Write, // Key difference: Write access
        });
    }

    fn requests_entity_id() -> bool {
        false
    }
}

unsafe impl QueryParam for Entity {
    type Item<'registry, 'state>
        = Entity
    // Entity itself doesn't have external lifetimes from the query
    where
        Self: 'registry;

    type Fetch<'registry, 'state>
        = FetchEntity
    where
        Self: 'registry;

    fn add_access(_descriptor: &mut Vec<ComponentAccessInfo>) {
        // Entity ID query does not access any components directly.
    }

    fn requests_entity_id() -> bool {
        true // This query parameter specifically requests the Entity ID.
    }
}

// TODO: Implement QueryParam for tuples
// unsafe impl<Q0: QueryParam, Q1: QueryParam, ...> QueryParam for (Q0, Q1, ...) { /* ... */ }
