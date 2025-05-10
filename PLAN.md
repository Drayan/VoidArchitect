# Design Plan: Story 4.2.1 - Implement Query Iterator Logic for Archetypes

## 1. Introduction

This document outlines the design for **Story 4.2.1: Implement Query Iterator Logic for Archetypes**. The goal is to design and implement the core internal iterator logic that can efficiently walk through archetypes containing entities that match a specific component query. This iterator must support immutable (`&Component`) and mutable (`&mut Component`) references, handle Rust's borrowing rules, and provide access to component data and entity IDs. This forms the foundation for the user-facing `registry.query()` API to be built in Story 4.2.2.

## 2. Core Concepts & Terminology

Inspired by Bevy ECS and common ECS patterns:

*   **`QueryParam` Trait:** A trait that will be implemented by types that can be fetched by a query (e.g., `&T`, `&mut T` where `T: Component`, `Entity`, `Option<&T>`, etc.). This trait will define metadata about the access (read/write, component type).
*   **`QueryDescriptor`:** An internal representation of a query, derived from the `QueryParam` tuple. It will store:
    *   A list of `ComponentAccessInfo` (Component ID, access type: Read/Write).
    *   Information about whether `Entity` ID is requested.
    *   It will be responsible for validating borrow rules at query definition time (e.g., no `&Health` and `&mut Health` for the same component type simultaneously).
*   **`Fetch<'world, 'state, W: QueryParam>` Trait:** An unsafe trait that defines how to retrieve the data for a specific `QueryParam` item (`W`) from a given archetype and entity index within that archetype. Implementations will handle the raw pointer access to component data.
*   **`ArchetypeFilter`:** Logic to determine if an archetype's component signature matches the requirements of a `QueryDescriptor`.

## 3. Data Structures

### 3.1. `QueryAccess` and `ComponentAccessInfo`

**Description:**
`AccessType` is an enum that explicitly defines whether a component is being accessed for reading (`Read`) or for writing/mutation (`Write`). This distinction is fundamental for enforcing Rust's borrowing rules within the ECS query system.

`ComponentAccessInfo` is a simple struct that pairs a `ComponentId` (uniquely identifying a component type) with its intended `AccessType` for a specific query. A query will typically involve multiple such `ComponentAccessInfo` items.

`QueryDescriptor` is a crucial internal structure that represents a fully analyzed and validated query. It's derived from the generic type parameters of a user's query (e.g., from `QueryIter<Q: QueryParam>` or the future user-facing query type). It contains a collection of `ComponentAccessInfo` detailing all components the query will access, and a flag indicating if the entity's `Entity` ID itself is requested.

**Goal:**
*   **`AccessType` & `ComponentAccessInfo`:**
    *   To provide clear, discrete units of information about how individual components are intended to be used by a query.
    *   To serve as building blocks for the `QueryDescriptor`.
*   **`QueryDescriptor`:**
    *   To provide a concrete, type-erased (regarding specific component types, using `ComponentId` instead) representation of the query's access patterns and requirements.
    *   To enable early and centralized validation of borrow rules *before* iteration begins. For example, it will check that a query doesn't request both `&Health` and `&mut Health` for the same `Health` component simultaneously, preventing data races and undefined behavior.
    *   To serve as the primary input for the archetype filtering process, allowing the `QueryIter` to efficiently identify only those archetypes that actually contain all the necessary components for the query.
    *   To guide the `Fetch` implementations on what specific data to retrieve from an archetype and what kind of access (read or write) is permitted.

```rust
// Potentially in shared/ecs/src/query/access.rs
use crate::component::ComponentId;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum AccessType {
    Read,
    Write,
}

#[derive(Debug, Clone, Copy)]
pub struct ComponentAccessInfo {
    pub component_id: ComponentId,
    pub access_type: AccessType,
}

// Describes the set of component accesses for a query
pub struct QueryDescriptor {
    pub components: Vec<ComponentAccessInfo>,
    pub requests_entity_id: bool,
    // Potentially other metadata like "Added<T>", "Changed<T>" in the future
}

impl QueryDescriptor {
    // fn new<Q: QueryParam>() -> Result<Self, QueryValidationError>
    // Validates that the query Q doesn't violate borrow rules (e.g. &T and &mut T of same component)
}
```

### 3.2. `QueryParam` Trait

**Description:**
The `QueryParam` trait is a cornerstone of the generic query system. It is an `unsafe trait` because its implementors, particularly their associated `Fetch` types, will be involved in unsafe memory operations to access component data. This trait acts as a "marker" and a provider of metadata for any type that can be legitimately requested as part of an ECS query.

Standard implementations will be provided for:
*   Immutable references to components (`&T where T: Component`).
*   Mutable references to components (`&mut T where T: Component`).
*   The `Entity` ID itself.
*   Optional component references (`Option<&T>`, `Option<&mut T>`).
*   Tuples of other `QueryParam` implementors (e.g., `(Entity, &Position, &mut Velocity)`), allowing complex queries.

Each implementation of `QueryParam` defines:
1.  `Item<'world, 'state>`: The actual type that the query iterator will yield for this part of the query (e.g., for `&Position`, the `Item` would be `&'world Position`).
2.  `Fetch`: An associated type that must implement the `Fetch` trait, specifying the low-level logic for retrieving this `Item`.
3.  `add_access()`: A static method used during query analysis to contribute this `QueryParam` part's component access requirements (e.g., read `Position`) to a `QueryDescriptor`.
4.  `requests_entity_id()`: A static method indicating if this `QueryParam` part specifically requests the `Entity` ID.

**Goal:**
*   To enable a highly generic and extensible query system. Users can define what data they need (e.g., `(Entity, &PlayerScore, &mut Health)`) using familiar Rust types and tuples.
*   To statically (at compile-time, during `QueryDescriptor` construction) collect all necessary information about component accesses (types, read/write permissions) required by a query. This is crucial for borrow checking and archetype filtering.
*   To link a high-level query parameter type (like `&Position`) to its specific, low-level data fetching logic (its associated `Fetch` implementation).
*   To allow the user-facing query API (Story 4.2.2) to be type-safe and ergonomic, while abstracting away the complex and `unsafe` pointer manipulation required for efficient component data access internally.
*   To provide a clear contract for extending the query system with new kinds of query parameters in the future (e.g., special filters, resources).

```rust
// Potentially in shared/ecs/src/query/world_query.rs
use crate::entity::Entity;
use crate::component::Component;
use crate::archetype::Archetype;
use super::access::ComponentAccessInfo;

// 'world and 'state are lifetime parameters for the world and query state
pub unsafe trait QueryParam {
    type Item<'world, 'state>; // The type returned by the iterator, e.g. (&'world Position, &'world mut Velocity)
    type Fetch: Fetch<'world, 'state, Self>;

    fn add_access(descriptor: &mut Vec<ComponentAccessInfo>);
    fn requests_entity_id() -> bool; // Does this part of the query request Entity ID?
}

// Example Implementations:
// unsafe impl<T: Component> QueryParam for &T { /* ... */ }
// unsafe impl<T: Component> QueryParam for &mut T { /* ... */ }
// unsafe impl QueryParam for Entity { /* ... */ }
// unsafe impl<Q0: QueryParam, Q1: QueryParam, ...> QueryParam for (Q0, Q1, ...) { /* ... */ }
```

### 3.3. `Fetch` Trait

**Description:**
The `Fetch` trait is an `unsafe` low-level trait responsible for the actual retrieval of data for a single `QueryParam` item from a specific entity within an archetype. It's tightly coupled with the `QueryParam` trait; each `QueryParam` implementor (e.g., `&T`, `&mut T`) has a corresponding `Fetch` implementation.

The `Fetch` trait's methods operate under the assumption that the calling context (primarily the `QueryIter`) has already:
1.  Validated that the query is safe in terms of borrowing rules (via `QueryDescriptor`).
2.  Ensured that the current archetype being processed actually contains the component(s) this `Fetch` instance is for.
3.  Provided a valid entity index within that archetype.

The `init_archetype` method allows a `Fetch` implementation to perform setup or caching when the `QueryIter` switches to a new archetype (e.g., caching the starting pointer for a component's data array in that archetype). The `fetch` method then uses this setup and the entity index to get a pointer to the specific component data for an entity and returns it as the `QueryParam::Item` (e.g., `&'world Position`).

**Goal:**
*   To encapsulate and isolate the `unsafe` memory access required to retrieve component data from an archetype's raw, contiguous storage. This keeps the rest of the query system, especially `QueryIter`, cleaner and focused on iteration logic.
*   To provide the concrete data item (e.g., `&'world Position` or `&'world mut Velocity`) for a given entity as requested by a part of a `QueryParam` tuple.
*   To allow for optimized data access, potentially including stateful fetching strategies via `init_archetype` if beneficial for performance (e.g., reducing redundant calculations when iterating many entities in the same archetype).
*   To act as the critical bridge between the abstract, type-safe `QueryParam` definitions used by the API and the concrete, untyped (or `Vec<u8>`) component storage within archetypes.
*   To ensure that the lifetimes (`'world`, `'state`) associated with the fetched data are correctly propagated, allowing Rust's borrow checker to validate their usage by the end-user of the query.

```rust
// Potentially in shared/ecs/src/query/fetch.rs

// Unsafe because it deals with raw pointers and assumes correct archetype/entity context
pub unsafe trait Fetch<'world, 'state, W: QueryParam + ?Sized> {
    // Initializes any state needed for fetching from a specific archetype.
    // Called when the iterator moves to a new archetype.
    fn init_archetype(archetype: &'world Archetype);

    // Fetches the data for the QueryParam item W from the current archetype
    // for the entity at the given index.
    // Panics if the archetype doesn't match or index is out of bounds.
    unsafe fn fetch(archetype: &'world Archetype, entity_index_in_archetype: usize) -> W::Item<'world, 'state>;
}
```

### 3.4. `QueryIter` (The Core Iterator for Story 4.2.1)

**Description:**
`QueryIter<'world, 'state, Q: QueryParam>` is the primary internal iterator that powers the ECS query system. This struct is central to Story 4.2.1. It is generic over a `QueryParam` type `Q`, which defines what data is being requested (e.g., `Q` could be `(Entity, &Position, &mut Velocity)`).

Upon creation, `QueryIter` performs several key steps:
1.  It derives a `QueryDescriptor` from the generic type `Q`. This descriptor outlines all component accesses and validates them against Rust's borrowing rules (e.g., no simultaneous `&T` and `&mut T` for the same component).
2.  It uses this `QueryDescriptor` to filter all archetypes in the `Registry`, creating an internal list of only those archetypes that actually contain all the components required by `Q`. This pre-filtering is crucial for efficiency.
3.  It then iterates entity by entity, first through all entities in the first matching archetype, then all entities in the second, and so on.

For each entity, it uses the `Fetch` implementations associated with each part of `Q` to retrieve the actual component data (or `Entity` ID). It then assembles these pieces of data into the `Q::Item` type (e.g., an `(Entity, &'world Position, &'world mut Velocity)` tuple) and yields it.

The lifetimes `'world` and `'state` are used to tie the lifetimes of the borrowed data to the lifetime of the `World` (or `Registry`) and any temporary query state, ensuring memory safety.

**Goal:**
*   To provide an efficient and safe mechanism for iterating over all entities that match the component and access requirements specified by the generic `QueryParam` type `Q`.
*   To correctly handle the iteration logic, including advancing through entities within an archetype and advancing to subsequent matching archetypes.
*   To orchestrate the use of the `Fetch` trait implementations to safely and correctly retrieve the requested data (e.g., `&Component`, `&mut Component`, `Entity` ID) for each entity from their respective archetypes.
*   To ensure that all operations adhere to Rust's borrowing rules for the data yielded by the iterator. This is achieved through a combination of the `QueryDescriptor`'s upfront validation and the correct management of lifetimes.
*   To serve as the direct, low-level data source that will be wrapped by the more ergonomic, user-facing query API (e.g., `registry.query::<Q>()`) planned for Story 4.2.2. It forms the "engine" of the query system.
*   To be highly optimized for iterating entities that match a query, by pre-filtering archetypes and enabling efficient, direct access to component data via the `Fetch` mechanism.

```rust
// Potentially in shared/ecs/src/query/iter.rs
use crate::registry::Registry;
use crate::archetype::{Archetype, ArchetypeId};
use super::world_query::QueryParam;
use super::access::QueryDescriptor;

pub struct QueryIter<'world, 'state, Q: QueryParam> {
    registry: &'world Registry,
    descriptor: QueryDescriptor, // Built from Q
    matching_archetypes: Vec<(&'world Archetype, ArchetypeId)>, // Pre-filtered list of archetypes
    current_archetype_idx: usize,
    current_entity_idx_in_archetype: usize,
    _phantom_query_state: std::marker::PhantomData<&'state ()>, // For 'state lifetime if needed by Fetch
    _phantom_query_type: std::marker::PhantomData<Q>,
}

impl<'world, 'state, Q: QueryParam> QueryIter<'world, 'state, Q> {
    pub(crate) fn new(registry: &'world Registry /*, descriptor: QueryDescriptor */) -> Self {
        // 1. Derive QueryDescriptor from Q (or take as param)
        // 2. Validate descriptor (borrow checks)
        // 3. Filter registry.archetypes() based on descriptor
        //    - Archetype must contain all components specified in descriptor.
        // Store matching archetypes.
        // Initialize indices.
        unimplemented!("QueryIter::new")
    }
}

impl<'world, 'state, Q: QueryParam> Iterator for QueryIter<'world, 'state, Q> {
    type Item = Q::Item<'world, 'state>; // e.g., (Entity, &Position, &mut Velocity)

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            if self.current_archetype_idx >= self.matching_archetypes.len() {
                return None; // No more matching archetypes
            }

            let (current_archetype, _archetype_id) = self.matching_archetypes[self.current_archetype_idx];

            if self.current_entity_idx_in_archetype < current_archetype.entity_count() {
                // SAFETY: Fetch implementations are unsafe and rely on QueryIter
                // to provide correct archetype and entity_index.
                // The QueryDescriptor validation ensures no aliasing &mut within Q.
                let item = unsafe {
                    Q::Fetch::fetch(current_archetype, self.current_entity_idx_in_archetype)
                };
                self.current_entity_idx_in_archetype += 1;
                return Some(item);
            } else {
                // Move to the next archetype
                self.current_archetype_idx += 1;
                self.current_entity_idx_in_archetype = 0;
                if self.current_archetype_idx < self.matching_archetypes.len() {
                    // Call init_archetype for the new archetype if Fetch needs it
                    // Q::Fetch::init_archetype(self.matching_archetypes[self.current_archetype_idx].0);
                }
                // Continue loop to try fetching from new archetype or exit
            }
        }
    }
}
```

## 4. Algorithm & Workflow (Story 4.2.1)

1.  **Query Definition (Conceptual for 4.2.1, actual API in 4.2.2):**
    A query is conceptually defined by a tuple of `QueryParam` types, e.g., `(Entity, &Position, &mut Velocity)`.

2.  **`QueryDescriptor` Generation & Validation:**
    *   From the `QueryParam` tuple, a `QueryDescriptor` is generated. This involves:
        *   Collecting `ComponentAccessInfo` for each component type.
        *   Noting if `Entity` is requested.
    *   The `QueryDescriptor` validates against borrow rules:
        *   No `&T` and `&mut T` for the same `ComponentId`.
        *   No multiple `&mut T` for the same `ComponentId`.
        *   If validation fails, query creation should error (this happens before iterator construction).

3.  **`QueryIter` Initialization:**
    *   Takes the `Registry` and the (validated) `QueryDescriptor` (or derives it from `Q`).
    *   Iterates through all archetypes in the `Registry`.
    *   For each archetype, checks if its component signature satisfies all requirements of the `QueryDescriptor` (i.e., contains all requested components).
    *   Stores references to all matching archetypes.
    *   Initializes `Fetch` states for the first matching archetype if necessary (e.g., `Q::Fetch::init_archetype()`).

4.  **`QueryIter::next()` Logic:**
    *   If the current archetype has more entities:
        *   Increment the entity index for the current archetype.
        *   Use `Q::Fetch::fetch()` to retrieve the data for the current entity. This involves:
            *   Each part of `Q` (e.g., `Entity`, `&Position`, `&mut Velocity`) has its own `Fetch` logic.
            *   `Fetch` for `&T` gets an immutable pointer to component `T` from the archetype's component storage for the entity.
            *   `Fetch` for `&mut T` gets a mutable pointer.
            *   `Fetch` for `Entity` gets the `Entity` ID.
        *   Return the assembled tuple.
    *   If the end of the current archetype is reached:
        *   Move to the next matching archetype in the pre-filtered list.
        *   Reset entity index to 0.
        *   Initialize `Fetch` states for the new archetype.
        *   If no more matching archetypes, return `None`.
    *   Repeat.

## 5. Component Storage & Safe Access in `Archetype`

*   Each `Archetype` will store component data in contiguous blocks, likely one `Vec<u8>` (or similar raw storage like `Box<[MaybeUninit<u8>]>`) per component type it contains.
*   It will need methods like:
    ```rust
    // In shared/ecs/src/archetype.rs
    impl Archetype {
        // Unsafe: Caller must ensure component_id is part of this archetype
        // and entity_idx is valid.
        unsafe fn get_component_ptr_readonly(&self, component_id: ComponentId, entity_idx: usize) -> *const u8;
        unsafe fn get_component_ptr_mutable(&self, component_id: ComponentId, entity_idx: usize) -> *mut u8;
        fn entity_count(&self) -> usize;
        fn entities(&self) -> &[Entity]; // To get Entity IDs
        fn contains_component(&self, component_id: ComponentId) -> bool;
    }
    ```
*   The `Fetch` implementations will use these methods, cast the `*const u8` / `*mut u8` to the correct `*const T` / `*mut T`, and then dereference to create `&T` / `&mut T`.
*   **Safety:**
    *   The primary safety mechanism is that the `QueryDescriptor` ensures no conflicting accesses *within a single query's returned tuple*.
    *   Rust's borrow checker ensures that lifetimes of the returned references (`&'world T`, `&'world mut T`) are respected by the user of the iterator.
    *   The iterator itself must not create aliasing mutable references. Since it processes one entity at a time, and the `QueryDescriptor` prevents aliasing within that entity's component tuple, this should be manageable.
    *   The `Fetch` trait is `unsafe` to implement, placing the burden of correct pointer manipulation on its implementers.

## 6. Code Structure (Proposed for `shared/ecs/src/`)

```
query/
├── mod.rs             // Public API for query module, re-exports
├── access.rs          // QueryDescriptor, ComponentAccessInfo, AccessType
├── world_query.rs     // QueryParam trait and its core impls (&T, &mut T, Entity, tuples)
├── fetch.rs           // Fetch trait and its core impls
└── iter.rs            // QueryIter struct and its Iterator impl
registry.rs            // Will use QueryIter in Story 4.2.2 for registry.query()
archetype.rs           // Needs methods for component pointer access
component.rs
entity.rs
lib.rs
```

## 7. Testing Strategy (Story 4.2.1)

*   **`QueryDescriptor`:**
    *   Test creation with valid `QueryParam` tuples.
    *   Test creation with invalid tuples (e.g., `(&T, &mut T)`) and ensure it errors/panics appropriately.
*   **Archetype Filtering:**
    *   Unit test the logic that filters archetypes based on a `QueryDescriptor`.
*   **`QueryIter` (Extensive Testing):**
    *   Query for a single read-only component (`&Position`).
    *   Query for a single mutable component (`&mut Velocity`).
    *   Query for `Entity`.
    *   Query for multiple components: `(&Position, &Velocity)`.
    *   Query for mixed mutability: `(&Position, &mut Velocity)`.
    *   Query including `Entity`: `(Entity, &Position)`.
    *   Scenario: No archetypes match the query. Iterator should be empty.
    *   Scenario: Archetypes match, but they have no entities. Iterator should be empty.
    *   Scenario: Multiple archetypes match, some with entities, some without.
    *   Verify correct component values are returned.
    *   Verify correct `Entity` IDs are returned.
    *   (Implicitly) Test that using the iterator doesn't cause borrow checker errors in test code, indicating lifetime and access correctness at a high level.

This revised plan for Story 4.2.1 focuses on building the complex internal machinery needed for flexible component-based queries. Story 4.2.2 will then build the ergonomic, type-safe public API on top of this foundation.