use super::registry_query::QueryParam;
use crate::archetype::{Archetype, Storage}; // Added Storage
use crate::component::{Component, ComponentTypeId}; // Added Component, ComponentTypeId
use crate::entity::Entity; // Added Entity
use std::marker::PhantomData; // Added PhantomData

// 'registry and 'state are lifetime parameters for the registry and query state
/// # Safety
/// Implementing this trait is unsafe because it involves raw pointer manipulation
/// and assumes that the `QueryIter` (or other calling context) has already
/// validated borrow rules and ensured that the archetype being accessed indeed
/// contains the component(s) this `Fetch` instance is designed for.
///
/// Implementors must ensure that:
/// 1. `init_archetype` correctly initializes any internal state based on the
///    provided `Archetype`. This method may cache pointers or offsets.
/// 2. `fetch` correctly and safely uses the `archetype` and `entity_index_in_archetype`
///    to access component data. It must not access data outside the bounds of
///    the component storage for the given archetype and entity index.
/// 3. The lifetimes `'registry` and `'state` are correctly handled and propagated
///    to the returned `W::Item`.
/// 4. The access (read or write) performed by `fetch` matches the access
///    declared by the corresponding `QueryParam`'s `add_access` method.
pub unsafe trait Fetch<'registry, 'state, W: QueryParam + ?Sized> {
    /// Initializes any state needed for fetching from a specific archetype.
    /// Called when the iterator moves to a new archetype.
    /// This method can be used to cache pointers or offsets to component data
    /// within the archetype for faster access in the `fetch` method.
    fn init_archetype(archetype: &'registry Archetype);

    /// Fetches the data for the QueryParam item `W` from the current archetype
    /// for the entity at the given `entity_index_in_archetype`.
    ///
    /// # Safety
    /// This method is unsafe because it performs raw pointer arithmetic to access
    /// component data. The caller (typically `QueryIter`) must ensure that:
    /// - The `archetype` is valid and contains the components required by `W`.
    /// - `entity_index_in_archetype` is a valid index within the `archetype`.
    /// - Borrowing rules are upheld (e.g., not calling `fetch` for mutable access
    ///   if an immutable borrow already exists for the same component and entity).
    unsafe fn fetch(
        archetype: &'registry Archetype,
        entity_index_in_archetype: usize,
    ) -> W::Item<'registry, 'state>;
}

/// A fetch implementation for reading a component `&T`.
#[derive(Debug)]
pub struct FetchRead<T: Component>(PhantomData<T>);

// The QueryParam type W is &'fetch_param T.
// 'fetch_param is the lifetime of the reference that defines the QueryParam.
// 'registry and 'state are the GAT lifetimes for the iterator item and fetch state.
unsafe impl<'registry, 'state, 'fetch_param, T: Component>
    Fetch<'registry, 'state, &'fetch_param T> for FetchRead<T>
where
    &'fetch_param T: 'registry, // Add this constraint
{
    fn init_archetype(_archetype: &'registry Archetype) {
        // For simple read, no archetype-specific state is typically needed here
        // beyond what `fetch` receives. If there were complex calculations
        // or pointer caching based on the archetype structure for this component type,
        // it would go here.
    }

    #[inline]
    unsafe fn fetch(
        archetype: &'registry Archetype,
        entity_index_in_archetype: usize,
    ) -> <&'fetch_param T as QueryParam>::Item<'registry, 'state> {
        // <&'fetch_param T as QueryParam>::Item<'registry, 'state> resolves to &'registry T

        // This assertion is important for safety. QueryIter should ensure this.
        debug_assert!(
            archetype.has_component_type(&ComponentTypeId::of::<T>()),
            "Archetype missing component for FetchRead"
        );

        let component_storage = archetype
            .get_storage_by_id(ComponentTypeId::of::<T>())
            .expect("FATAL: Archetype selected by QueryIter is missing expected component storage. This indicates a bug in QueryIter's archetype filtering.");

        let typed_storage = component_storage
            .as_any()
            .downcast_ref::<Storage<T>>()
            .expect("FATAL: Failed to downcast to Storage<T>. Component type mismatch or internal ECS error.");

        // Bounds check should be implicitly handled if entity_index_in_archetype is correct
        // and archetype.entity_count() is respected by QueryIter.
        // For added safety, especially during development:
        // We use as_slice() which is a public method on Storage<T>.
        let components_slice = typed_storage.as_slice();
        debug_assert!(
            entity_index_in_archetype < components_slice.len(),
            "Entity index out of bounds for component storage."
        );

        &components_slice[entity_index_in_archetype]
    }
}

/// A fetch implementation for mutably accessing a component `&mut T`.
#[derive(Debug)]
pub struct FetchWrite<T: Component>(PhantomData<T>);

unsafe impl<'registry, 'state, 'fetch_param, T: Component>
    Fetch<'registry, 'state, &'fetch_param mut T> for FetchWrite<T>
where
    &'fetch_param mut T: 'registry, // Add this constraint
{
    fn init_archetype(_archetype: &'registry Archetype) {
        // Similar to FetchRead, no archetype-specific state needed for simple mutable access
        // beyond what `fetch` receives and processes.
    }

    #[inline]
    unsafe fn fetch(
        archetype: &'registry Archetype,
        entity_index_in_archetype: usize,
    ) -> <&'fetch_param mut T as QueryParam>::Item<'registry, 'state> {
        // <&'fetch_param mut T as QueryParam>::Item<'registry, 'state> resolves to &'registry mut T

        // This assertion is important for safety. QueryIter should ensure this.
        debug_assert!(
            archetype.has_component_type(&ComponentTypeId::of::<T>()),
            "Archetype missing component for FetchWrite"
        );

        // Instead of trying to cast &Archetype to &mut Archetype (which is invalid in Rust),
        // we need to use raw pointers to access the component data directly.
        
        // SAFETY: This is safe because:
        // 1. QueryDescriptor ensures only one mutable access to this component type
        // 2. The query system guarantees non-aliasing access during iteration
        // 3. The entity_index_in_archetype is valid (guaranteed by QueryIter)
        
        // Get a reference to the storage
        let storage = archetype
            .get_storage_by_id(ComponentTypeId::of::<T>())
            .expect("FATAL: Archetype selected by QueryIter is missing expected component storage. This indicates a bug in QueryIter's archetype filtering.");
        
        // Downcast to the specific Storage<T>
        let typed_storage = storage
            .as_any()
            .downcast_ref::<Storage<T>>()
            .expect("FATAL: Failed to downcast to Storage<T>. Component type mismatch or internal ECS error.");
        
        // Get a pointer to the components slice
        let components_ptr = typed_storage.as_slice().as_ptr();
        
        // Calculate the pointer to the specific component
        // SAFETY: entity_index_in_archetype is guaranteed to be in bounds by QueryIter
        let component_ptr = unsafe { components_ptr.add(entity_index_in_archetype) } as *mut T;
        
        // Create a mutable reference
        // SAFETY: We know this is safe because:
        // 1. The pointer is valid (from a valid slice)
        // 2. The query system guarantees exclusive access to this component
        unsafe { &mut *component_ptr }
    }
}

/// A fetch implementation for retrieving the Entity ID.
#[derive(Debug)]
pub struct FetchEntity;

// Note: Entity itself is the QueryParam type W.
// It does not have a 'fetch_param lifetime like &T or &mut T.
unsafe impl<'registry, 'state> Fetch<'registry, 'state, Entity> for FetchEntity {
    fn init_archetype(_archetype: &'registry Archetype) {
        // No archetype-specific state needed for fetching Entity ID.
    }

    #[inline]
    unsafe fn fetch(
        archetype: &'registry Archetype,
        entity_index_in_archetype: usize,
    ) -> <Entity as QueryParam>::Item<'registry, 'state> {
        // <Entity as QueryParam>::Item<'registry, 'state> resolves to Entity

        // QueryIter ensures entity_index_in_archetype is valid.
        // Archetype::get_entity_by_row handles Option for safety, but here we expect it.
        archetype.get_entity_by_row(entity_index_in_archetype)
            .expect("FATAL: Failed to get entity by row. QueryIter filtering or Archetype internal state inconsistent.")
    }
}
