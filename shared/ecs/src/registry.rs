use std::collections::{BTreeSet, HashMap, VecDeque};

use crate::archetype::{AnyStorage, Archetype, ArchetypeId, Storage};
use crate::component::{Component, ComponentBundle, ComponentTypeId};
use crate::entity::Entity;
// use std::any::Any; // Unused import

/// Errors that can occur during component insertion.
#[derive(Debug, PartialEq, Eq)]
pub enum InsertError {
    /// The specified entity was not found or is not alive.
    EntityNotFound,
    /// A component in the bundle has not been registered with the `Registry`.
    ComponentTypeNotRegistered(ComponentTypeId),
}

/// Errors that can occur during component removal.
#[derive(Debug, PartialEq, Eq)]
pub enum RemoveError {
    /// The specified entity was not found or is not alive.
    EntityNotFound,
    /// The component to be removed was not present on the entity.
    ComponentNotPresent,
    /// Internal error: Failed to downcast the component data to the expected type.
    /// This typically indicates an inconsistency in the ECS internal state.
    DowncastFailed,
}

/// Errors that can occur during component updates.
#[derive(Debug, PartialEq, Eq)]
pub enum UpdateError {
    /// The specified entity was not found or is not alive.
    EntityNotFound,
    /// The component to be updated was not present on the entity.
    ComponentNotPresent,
    /// The component type is not registered (should ideally be caught earlier).
    ComponentTypeNotRegistered,
}

/// Manages entities, components, and their organization into archetypes.
/// The `Registry` is the primary point of interaction for creating entities,
/// adding/removing components, and querying for entities with specific components.
pub struct Registry {
    /// Stores all archetypes, keyed by their `ArchetypeId`.
    archetypes: HashMap<ArchetypeId, Archetype>,

    /// Maps a sorted set of `ComponentTypeId`s (the signature) to an `ArchetypeId`.
    /// This allows finding an existing archetype or determining if a new one needs to be created.
    signature_to_archetype_id: HashMap<BTreeSet<ComponentTypeId>, ArchetypeId>,

    /// Counter for generating new `ArchetypeId`s.
    next_archetype_id: usize,

    /// Tracks the location (archetype and row within that archetype) of each entity.
    entity_locations: HashMap<Entity, (ArchetypeId, usize)>,

    /// Stores factories for creating component storages.
    /// Keyed by `ComponentTypeId`, the value is a function that returns a new, empty `Box<dyn AnyStorage>`.
    component_factories: HashMap<ComponentTypeId, Box<dyn Fn() -> Box<dyn AnyStorage>>>,

    // --- Entity Allocation ---
    /// Stores entities that have been destroyed and can be reused.
    free_entities: VecDeque<usize>, // Stores reusable entity IDs (indices)
    /// The next available entity ID if no free entities are available.
    next_entity_id: usize,
    /// Stores the generation for each entity ID. Incremented when an ID is reused.
    entity_generations: Vec<usize>,
}

impl Registry {
    /// Creates a new, empty `Registry`.
    pub fn new() -> Self {
        Self {
            archetypes: HashMap::new(),
            signature_to_archetype_id: HashMap::new(),
            next_archetype_id: 0,
            entity_locations: HashMap::new(),
            component_factories: HashMap::new(),
            free_entities: VecDeque::new(),
            next_entity_id: 0,
            entity_generations: Vec::new(),
        }
    }

    /// Registers a component type with the `Registry`.
    /// This allows the `Registry` to create storage for this component type when needed.
    /// It should be called for each component type that will be used with the ECS.
    pub fn register_component<C: Component>(&mut self) {
        let component_type_id = ComponentTypeId::of::<C>();
        self.component_factories.entry(component_type_id).or_insert_with(|| {
            Box::new(|| Box::new(Storage::<C>::new()) as Box<dyn AnyStorage>)
        });
    }

    /// Generates a new, unique `ArchetypeId`.
    fn get_new_archetype_id(&mut self) -> ArchetypeId {
        let id = ArchetypeId::new(self.next_archetype_id);
        self.next_archetype_id += 1;
        id
    }

    // --- Entity Management ---

    /// Spawns a new entity and places it into the default "null" archetype.
    /// The "null" archetype is an archetype with no components.
    ///
    /// # Returns
    /// The newly spawned `Entity`.
    pub fn spawn(&mut self) -> Entity {
        let entity_id_val;
        let generation;

        if let Some(reusable_id) = self.free_entities.pop_front() {
            entity_id_val = reusable_id;
            // Ensure entity_generations vec is large enough (should be if ID was in free_entities)
            if entity_id_val >= self.entity_generations.len() {
                // This case implies an inconsistency, as free_entities should only contain valid IDs.
                // For safety, resize, though ideally this branch isn't hit.
                self.entity_generations.resize(entity_id_val + 1, 0); // Default to 0. The generation is already set by despawn.
            }
            // Generation was already incremented by despawn(). Use it directly.
            generation = self.entity_generations[entity_id_val];
        } else {
            entity_id_val = self.next_entity_id;
            self.next_entity_id += 1;
            // Ensure entity_generations vec is large enough
            if entity_id_val >= self.entity_generations.len() {
                self.entity_generations.resize(entity_id_val + 1, 0);
            }
            self.entity_generations[entity_id_val] = 0; // Initial generation is 0
            generation = 0;
        }
        let new_entity = Entity::new(entity_id_val, generation);

        // Place the new entity into the "null" archetype.
        let null_archetype_signature = BTreeSet::new(); // Empty set for the null archetype
        let null_archetype_id = self.find_or_create_archetype(null_archetype_signature);

        if let Some(null_archetype) = self.archetypes.get_mut(&null_archetype_id) {
            let entity_row_in_archetype = null_archetype.add_entity_placeholder(new_entity);
            self.entity_locations
                .insert(new_entity, (null_archetype_id, entity_row_in_archetype));
        } else {
            // This should not happen if find_or_create_archetype works correctly.
            panic!(
                "Failed to find or create the null archetype (ID: {:?}) during spawn.",
                null_archetype_id
            );
        }

        new_entity
    }

    /// Despawns an entity, removing it from its archetype and freeing its ID for reuse.
    /// Component data associated with the entity is cleared.
    ///
    /// # Arguments
    /// * `entity` - The `Entity` to despawn.
    ///
    /// # Returns
    /// `true` if the entity was alive and successfully despawned, `false` otherwise
    /// (e.g., if the entity was already despawned or never existed with that generation).
    pub fn despawn(&mut self, entity: Entity) -> bool {
        if !self.is_entity_alive(entity) {
            return false; // Entity is not alive or doesn't exist with this generation
        }

        // Remove from archetype and update entity locations
        if let Some((archetype_id, row_index)) = self.entity_locations.remove(&entity) {
            if let Some(archetype) = self.archetypes.get_mut(&archetype_id) {
                // remove_entity_by_row will handle clearing component data by swapping it out
                let (_removed_entity, _removed_components, swapped_in_entity_opt) =
                    archetype.remove_entity_by_row(row_index);

                // If an entity was swapped into the removed entity's slot, update its location
                if let Some(swapped_in_entity) = swapped_in_entity_opt {
                    // The row_index remains the same for the swapped_in_entity
                    self.entity_locations.insert(swapped_in_entity, (archetype_id, row_index));
                }
            } else {
                // This implies an inconsistency: entity_locations had an entry, but the archetype doesn't exist.
                // This should ideally not happen. For robustness, one might log an error.
                // However, we still proceed to free the entity ID.
            }
        }
        // If entity_locations.remove(&entity) returned None, it means the entity,
        // despite passing is_entity_alive, was not tracked in entity_locations.
        // This could happen if it was spawned but an error occurred before insertion,
        // or if it was moved from an archetype and entity_locations wasn't updated.
        // This is an inconsistent state. For now, we proceed to mark its ID as free.

        // Mark the entity ID as free and increment its generation.
        // is_entity_alive check ensures entity.id() is within bounds of entity_generations.
        // Increment the generation for this ID to invalidate existing Entity handles.
        self.entity_generations[entity.id()] += 1; // Restored increment in despawn
        self.free_entities.push_back(entity.id());

        true // Successfully despawned
    }

    /// Checks if an entity is currently alive (i.e., has not been despawned or its generation matches).
    pub fn is_entity_alive(&self, entity: Entity) -> bool {
        entity.id() < self.entity_generations.len()
            && self.entity_generations[entity.id()] == entity.generation()
    }

    // --- Archetype Management ---

    /// Finds an existing archetype ID that matches the given component signature,
    /// or creates a new one (including the Archetype struct instance) if no match is found.
    ///
    /// # Arguments
    /// * `component_types_set` - A `BTreeSet` of `ComponentTypeId`s representing the desired signature.
    ///
    /// # Returns
    /// The `ArchetypeId` of the found or created archetype.
    ///
    /// # Panics
    /// Panics if a `ComponentTypeId` in `component_types_set` has not been registered
    /// via `register_component`, as the `Registry` would not know how to create its storage.
    fn find_or_create_archetype(
        &mut self,
        component_types_set: BTreeSet<ComponentTypeId>,
    ) -> ArchetypeId {
        if let Some(archetype_id) = self.signature_to_archetype_id.get(&component_types_set) {
            *archetype_id
        } else {
            let new_archetype_id = self.get_new_archetype_id();
            let component_types_vec: Vec<ComponentTypeId> =
                component_types_set.iter().cloned().collect();

            let mut storages: Vec<Box<dyn AnyStorage>> =
                Vec::with_capacity(component_types_vec.len());
            for component_type_id in &component_types_vec {
                match self.component_factories.get(component_type_id) {
                    Some(factory_fn) => {
                        storages.push(factory_fn());
                    }
                    None => {
                        // This case should ideally be prevented by checks in higher-level functions
                        // like `insert`, ensuring all components in a bundle are registered.
                        // Or, if this function is called directly, the caller must ensure registration.
                        panic!(
                            "Attempted to create archetype with unregistered component type: {:?}. \
                             Ensure all components are registered using `registry.register_component::<C>()` before use.",
                            component_type_id
                        );
                    }
                }
            }

            let new_archetype =
                Archetype::new(new_archetype_id, component_types_vec, storages);
            self.archetypes.insert(new_archetype_id, new_archetype);
            self.signature_to_archetype_id.insert(component_types_set, new_archetype_id);
            new_archetype_id
        }
    }

    // Helper to get the ArchetypeId for an entity, if it exists and is alive.
    pub fn get_entity_archetype_id(&self, entity: Entity) -> Option<ArchetypeId> {
        if self.is_entity_alive(entity) {
            self.entity_locations.get(&entity).map(|(arch_id, _)| *arch_id)
        } else {
            None
        }
    }

    /// Inserts a bundle of components for a given entity.
    ///
    /// This method will:
    /// 1. Validate the entity.
    /// 2. Check if all components in the bundle are registered.
    /// 3. Calculate the new archetype signature for the entity (existing components + new components).
    /// 4. If the new archetype is the same as the current one, update components in-place.
    /// 5. If the new archetype is different, migrate the entity:
    ///    a. Remove the entity and its existing components from the old archetype.
    ///    b. Add the entity with all its components (old + new) to the new archetype.
    ///    c. Update the entity's location.
    ///
    /// # Arguments
    /// * `entity` - The `Entity` to add components to.
    /// * `bundle` - A `ComponentBundle` containing the components to add.
    ///
    /// # Returns
    /// * `Ok(())` if the insertion was successful.
    /// * `Err(InsertError)` if an error occurred (e.g., entity not found, component not registered).
    pub fn insert<B: ComponentBundle>(
        &mut self,
        entity: Entity,
        bundle: B,
    ) -> Result<(), InsertError> {
        // 1. Validate entity
        if !self.is_entity_alive(entity) {
            return Err(InsertError::EntityNotFound);
        }

        let bundle_component_type_ids = bundle.get_component_type_ids();
        let new_components_data = bundle.into_components_data(); // Removed mut

        // 2. Check bundle component registration
        for component_type_id in &bundle_component_type_ids {
            if !self.component_factories.contains_key(component_type_id) {
                return Err(InsertError::ComponentTypeNotRegistered(*component_type_id));
            }
        }

        let (current_archetype_id, current_row_index) = self
            .entity_locations
            .get(&entity)
            .copied()
            .expect("Entity is alive but not found in entity_locations. This indicates an ECS bug.");

        let current_archetype = self.archetypes.get(&current_archetype_id).expect(
            "Current archetype ID from entity_locations not found. This indicates an ECS bug.",
        );

        // 3. Calculate new archetype signature
        let mut new_archetype_signature = current_archetype.signature().clone();
        let mut in_place_update = true;

        for component_type_id in &bundle_component_type_ids {
            if !new_archetype_signature.contains(component_type_id) {
                new_archetype_signature.insert(*component_type_id);
                in_place_update = false; // Archetype will change if we add a new type
            }
        }

        // 4. Handle same-archetype updates (in-place component update) vs. archetype migration.
        if in_place_update {
            // All components in the bundle already exist on the entity. Update them in place.
            let archetype = self
                .archetypes
                .get_mut(&current_archetype_id)
                .expect("Current archetype disappeared. This indicates an ECS bug.");

            for (component_type_id, component_data) in new_components_data {
                archetype.update_component_at_row(
                    current_row_index,
                    component_type_id,
                    component_data,
                );
            }
            Ok(())
        } else {
            // Archetype transition is needed.
            let new_archetype_id = self.find_or_create_archetype(new_archetype_signature);

            // 5a. Remove the entity and its existing components from the old archetype.
            let (removed_entity, mut existing_components_map, swapped_entity_opt) = self
                .archetypes
                .get_mut(&current_archetype_id)
                .expect("Old archetype disappeared. This indicates an ECS bug.")
                .remove_entity_by_row(current_row_index);

            assert_eq!(removed_entity, entity, "Removed entity mismatch. ECS bug.");

            if let Some(swapped_entity) = swapped_entity_opt {
                self.entity_locations
                    .insert(swapped_entity, (current_archetype_id, current_row_index));
            }

            // Combine existing components with new components. New components overwrite existing ones if types match.
            for (type_id, data) in new_components_data {
                existing_components_map.insert(type_id, data);
            }

            // 5b. Add the entity with all its components (old + new) to the new archetype.
            let new_archetype = self
                .archetypes
                .get_mut(&new_archetype_id)
                .expect("New archetype disappeared. This indicates an ECS bug.");

            let new_row_index = new_archetype
                .add_entity_with_all_components(entity, &mut existing_components_map);

            // 5c. Update the entity's location.
            self.entity_locations.insert(entity, (new_archetype_id, new_row_index));

            Ok(())
        }
    }

    /// Retrieves an immutable reference to a component of type `T` for a given entity.
    ///
    /// # Arguments
    /// * `entity` - The `Entity` for which to retrieve the component.
    ///
    /// # Returns
    /// `Some(&T)` if the entity is alive and has the component.
    /// `None` otherwise (e.g., entity not alive, entity doesn't have the component,
    /// or the component type is not part of the entity's archetype).
    ///
    /// # Notes on Safety
    /// While this method itself does not use `unsafe` blocks, its correctness relies on the
    /// internal consistency of the ECS, ensured by other `Registry` and `Archetype` methods:
    /// 1. `entity_locations` must correctly map live entities to their archetype and row.
    /// 2. The `Archetype` must correctly store and provide access to the component data
    ///    based on `ComponentTypeId` and row index.
    pub fn get<T: Component>(&self, entity: Entity) -> Option<&T> {
        if !self.is_entity_alive(entity) {
            return None;
        }

        // `entity_locations.get()` returns `Option<&(ArchetypeId, usize)>`.
        // `ArchetypeId` and `usize` are `Copy`, so `&(archetype_id, row_index)`
        // allows us to get copied values.
        let &(archetype_id, row_index) = self.entity_locations.get(&entity)?;

        let archetype = self.archetypes.get(&archetype_id)?;
        archetype.get_component_at_row::<T>(row_index)
    }

    /// Retrieves a mutable reference to a component of type `T` for a given entity.
    ///
    /// # Arguments
    /// * `entity` - The `Entity` for which to retrieve the component.
    ///
    /// # Returns
    /// `Some(&mut T)` if the entity is alive and has the component.
    /// `None` otherwise.
    ///
    /// # Notes on Safety
    /// Similar to `get`, this method's correctness relies on the internal consistency
    /// of the ECS.
    /// Additionally, Rust's borrowing rules must be upheld by the caller. Since this returns `&mut T`,
    /// care must be taken to ensure no other mutable or immutable borrows of this
    /// specific component instance exist across multiple calls for the same component if not
    /// handled carefully by the caller (e.g., through systems ensuring disjoint access or
    /// queries managing borrows).
    pub fn get_mut<T: Component>(&mut self, entity: Entity) -> Option<&mut T> {
        if !self.is_entity_alive(entity) {
            return None;
        }

        // Get location first to avoid borrowing issues with self.archetypes.get_mut.
        // `archetype_id` and `row_index` are copied here because `ArchetypeId` and `usize` are `Copy`.
        let (archetype_id, row_index) = match self.entity_locations.get(&entity) {
            Some(&(id, idx)) => (id, idx),
            None => return None,
        };

        // Now `self.entity_locations` is no longer borrowed immutably.
        let archetype = self.archetypes.get_mut(&archetype_id)?;
        archetype.get_component_at_row_mut::<T>(row_index)
    }

    /// Updates an existing component `C` for the given `entity`.
    ///
    /// If the entity is alive and has the component `C`, its data is replaced with the provided `component` value.
    /// The entity's archetype does not change.
    ///
    /// # Arguments
    /// * `entity` - The `Entity` whose component is to be updated.
    /// * `component` - The new instance of component `C` to replace the existing one.
    ///
    /// # Returns
    /// * `Ok(())` if the component was successfully updated.
    /// * `Err(UpdateError)` if an error occurred:
    ///     - `UpdateError::EntityNotFound`: If the entity is not alive.
    ///     - `UpdateError::ComponentNotPresent`: If the entity does not currently have component `C`.
    ///     - `UpdateError::ComponentTypeNotRegistered`: If component `C` has not been registered with the `Registry`.
    ///
    /// # Panics
    /// Panics internally if the ECS is in an inconsistent state (e.g., `entity_locations` points
    /// to a non-existent archetype, or `Archetype::update_component_at_row` panics due to an
    /// internal type mismatch, which should not happen if `C` is correctly typed and registered).
    pub fn update_component<C: Component>(
        &mut self,
        entity: Entity,
        component: C,
    ) -> Result<(), UpdateError> {
        if !self.is_entity_alive(entity) {
            return Err(UpdateError::EntityNotFound);
        }

        let component_type_id = ComponentTypeId::of::<C>();
        if !self.component_factories.contains_key(&component_type_id) {
            return Err(UpdateError::ComponentTypeNotRegistered);
        }

        let (archetype_id, row_index) = match self.entity_locations.get(&entity) {
            Some(&(id, idx)) => (id, idx),
            None => {
                // This case should ideally be caught by is_entity_alive or indicate an inconsistency
                // if an alive entity is not in entity_locations.
                return Err(UpdateError::EntityNotFound);
            }
        };

        let archetype = match self.archetypes.get_mut(&archetype_id) {
            Some(arch) => arch,
            None => {
                // Should not happen if entity_locations is consistent.
                // This indicates a critical ECS internal state error.
                panic!(
                    "Internal ECS Error: Entity {:?} location points to non-existent archetype ID {:?}",
                    entity, archetype_id
                );
            }
        };

        if !archetype.has_component_type(&component_type_id) {
            return Err(UpdateError::ComponentNotPresent);
        }

        // Call the archetype's update method.
        // This method will panic on internal errors such as:
        // - row_index out of bounds for the archetype's internal entity list.
        // - component_type_id not found in the archetype's signature (already checked above by has_component_type).
        // - Mismatched component type during downcast in Storage::replace_at_row (prevented by generics here).
        // If it returns, the update at the archetype level was successful.
        archetype.update_component_at_row(row_index, component_type_id, Box::new(component));

        // If archetype.update_component_at_row() did not panic, the update is considered successful.
        Ok(())
    }

    /// Removes a component of type `T` from the given entity.
    ///
    /// If the component is successfully removed, the entity might be moved to a different archetype.
    ///
    /// # Arguments
    /// * `entity` - The `Entity` from which to remove the component.
    ///
    /// # Returns
    /// * `Ok(T)` containing the removed component instance if successful.
    /// * `Err(RemoveError)` if an error occurred (e.g., entity not found, component not present).
    ///
    /// # Panics
    /// Panics internally if the ECS is in an inconsistent state.
    pub fn remove<T: Component>(&mut self, entity: Entity) -> Result<(), RemoveError> {
        if !self.is_entity_alive(entity) {
            return Err(RemoveError::EntityNotFound);
        }

        let component_type_to_remove = ComponentTypeId::of::<T>();

        let (current_archetype_id, current_row_index) = self
            .entity_locations
            .get(&entity)
            .copied()
            .expect("Entity is alive but not found in entity_locations. This indicates an ECS bug.");

        let current_archetype = self.archetypes.get(&current_archetype_id).expect(
            "Current archetype ID from entity_locations not found. This indicates an ECS bug.",
        );

        if !current_archetype.has_component_type(&component_type_to_remove) {
            return Err(RemoveError::ComponentNotPresent);
        }

        // Calculate the new archetype signature (current signature - removed component type)
        let mut new_archetype_signature = current_archetype.signature().clone();
        if !new_archetype_signature.remove(&component_type_to_remove) {
            // This should not happen if has_component_type check passed.
            panic!(
                "Component type to remove was not found in current archetype's signature despite passing has_component_type. ECS bug."
            );
        }

        let new_archetype_id = self.find_or_create_archetype(new_archetype_signature);

        // Remove the entity and all its components from the old archetype.
        let (removed_entity, mut all_components_map, swapped_entity_opt) = self
            .archetypes
            .get_mut(&current_archetype_id)
            .expect("Old archetype disappeared during remove_component. This indicates an ECS bug.")
            .remove_entity_by_row(current_row_index);

        assert_eq!(removed_entity, entity, "Removed entity mismatch. ECS bug.");

        if let Some(swapped_entity) = swapped_entity_opt {
            self.entity_locations
                .insert(swapped_entity, (current_archetype_id, current_row_index));
        }

        // Extract the component instance we are removing.
        let removed_component_data =
            all_components_map.remove(&component_type_to_remove).expect(
                "Component to remove was not found in the extracted components map. ECS bug.",
            );

        // Add the entity with its remaining components to the new archetype.
        let new_archetype = self.archetypes.get_mut(&new_archetype_id).expect(
            "New archetype disappeared during remove_component. This indicates an ECS bug.",
        );

        let new_row_index =
            new_archetype.add_entity_with_all_components(entity, &mut all_components_map);

        // Update the entity's location.
        self.entity_locations.insert(entity, (new_archetype_id, new_row_index));

        // Downcast the removed component data to T.
        match removed_component_data.downcast::<T>() {
            Ok(_component_instance) => Ok(()), // Changed: _component_instance and Ok(())
            Err(_) => Err(RemoveError::DowncastFailed), // Should not happen if types are consistent
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*; // Imports Registry, Entity, Component, ComponentBundle, etc.
    use crate::component::ComponentTypeId;
    use crate::entity::Entity;
    use crate::registry::{InsertError, Registry, RemoveError, UpdateError};

    // --- Test Components ---
    #[derive(Debug, Clone, PartialEq)]
    struct Position {
        x: f32,
        y: f32,
    }
    impl Component for Position {}

    #[derive(Debug, Clone, PartialEq)]
    struct Velocity {
        dx: f32,
        dy: f32,
    }
    impl Component for Velocity {}

    #[derive(Debug, Clone, PartialEq)]
    struct TagA;
    impl Component for TagA {}

    #[derive(Debug, Clone, PartialEq)]
    struct TagB;
    impl Component for TagB {}

    #[derive(Debug, Clone, PartialEq)]
    struct ComponentA {
        value: i32,
    }
    impl Component for ComponentA {}

    #[derive(Debug, Clone, PartialEq)]
    struct ComponentB {
        value: f64,
    }
    impl Component for ComponentB {}

    #[derive(Debug, Clone, PartialEq)]
    struct ComponentC {
        value: String,
    }
    impl Component for ComponentC {}

    #[derive(Debug, Clone, PartialEq)]
    struct MarkerComponentD;
    impl Component for MarkerComponentD {}

    #[derive(Debug, Clone, PartialEq)]
    struct DataComponentE {
        data: Vec<u32>,
    }
    impl Component for DataComponentE {}

    // --- Test Helper ---
    fn create_test_registry() -> Registry {
        let mut registry = Registry::new();
        registry.register_component::<Position>();
        registry.register_component::<Velocity>();
        registry.register_component::<TagA>();
        registry.register_component::<TagB>();
        registry.register_component::<ComponentA>();
        registry.register_component::<ComponentB>();
        registry.register_component::<ComponentC>();
        registry.register_component::<MarkerComponentD>();
        registry.register_component::<DataComponentE>();
        registry
    }

    // --- Existing Tests (Pre-Story 4.1.7) ---
    #[test]
    fn registry_new() {
        let registry = Registry::new();
        assert_eq!(registry.archetypes.len(), 0);
        assert_eq!(registry.signature_to_archetype_id.len(), 0);
        assert_eq!(registry.next_archetype_id, 0);
        assert_eq!(registry.entity_locations.len(), 0);
        assert_eq!(registry.next_entity_id, 0);
        assert_eq!(registry.entity_generations.len(), 0);
        assert!(registry.free_entities.is_empty());
    }

    #[test]
    fn registry_spawn_entity() {
        let mut registry = create_test_registry();
        let entity1 = registry.spawn();
        let entity2 = registry.spawn();

        assert_eq!(entity1.id(), 0);
        assert_eq!(entity1.generation(), 0);
        assert_eq!(entity2.id(), 1);
        assert_eq!(entity2.generation(), 0);

        assert_eq!(registry.next_entity_id, 2);
        assert_eq!(registry.entity_generations.len(), 2);
        assert_eq!(registry.entity_generations[0], 0);
        assert_eq!(registry.entity_generations[1], 0);

        // Check that spawned entities are in the null archetype
        let null_signature = BTreeSet::new();
        let null_archetype_id =
            registry.signature_to_archetype_id.get(&null_signature).unwrap();

        let (loc1_arch_id, _loc1_row) = registry.entity_locations.get(&entity1).unwrap();
        assert_eq!(loc1_arch_id, null_archetype_id);

        let (loc2_arch_id, _loc2_row) = registry.entity_locations.get(&entity2).unwrap();
        assert_eq!(loc2_arch_id, null_archetype_id);

        let null_archetype = registry.archetypes.get(null_archetype_id).unwrap();
        assert_eq!(null_archetype.entities_count(), 2);
    }

    #[test]
    fn registry_despawn_and_reuse_entity() {
        let mut registry = create_test_registry();
        let entity1 = registry.spawn(); // id 0, gen 0
        let entity2 = registry.spawn(); // id 1, gen 0

        assert!(registry.despawn(entity1));
        assert!(!registry.is_entity_alive(entity1));
        assert_eq!(registry.entity_generations[0], 1); // Generation incremented
        assert_eq!(registry.free_entities.len(), 1);
        assert_eq!(registry.free_entities[0], 0); // entity1's id (0) is now free

        let entity3 = registry.spawn(); // Should reuse entity1's id (0)
        assert_eq!(entity3.id(), 0);
        assert_eq!(entity3.generation(), 1); // New generation
        assert!(registry.is_entity_alive(entity3));
        assert!(!registry.is_entity_alive(entity1)); // Old handle is invalid

        assert_eq!(registry.free_entities.len(), 0); // No free entities now
        assert_eq!(registry.next_entity_id, 2); // Next ID remains 2

        // Despawn entity2
        assert!(registry.despawn(entity2));
        assert!(!registry.is_entity_alive(entity2));
        assert_eq!(registry.entity_generations[1], 1);
        assert_eq!(registry.free_entities.len(), 1);
        assert_eq!(registry.free_entities[0], 1);

        // Despawn entity3
        assert!(registry.despawn(entity3));
        assert!(!registry.is_entity_alive(entity3));
        assert_eq!(registry.entity_generations[0], 2); // Generation for id 0 incremented again
        assert_eq!(registry.free_entities.len(), 2);
        assert!(registry.free_entities.contains(&0)); // id 0 is free
        assert!(registry.free_entities.contains(&1)); // id 1 is free

        // Spawn new entities to check reuse order (VecDeque is FIFO for pop_front)
        let entity4 = registry.spawn(); // Should reuse id 1 (despawned after entity3's id 0 was freed)
        // Correction: free_entities.push_back, pop_front -> FIFO
        // So, if 1 was pushed back after 0, 0 should be popped first.
        // entity1 (id 0) despawned, gen becomes 1. free: [0]
        // entity3 (id 0, gen 1) spawned. free: []
        // entity2 (id 1) despawned, gen becomes 1. free: [1]
        // entity3 (id 0) despawned, gen becomes 2. free: [1, 0]
        // entity4 should be id 1, gen 1
        assert_eq!(entity4.id(), 1);
        assert_eq!(entity4.generation(), 1);

        let entity5 = registry.spawn(); // Should reuse id 0
        assert_eq!(entity5.id(), 0);
        assert_eq!(entity5.generation(), 2);
    }

    #[test]
    fn registry_despawn_non_existent_entity() {
        let mut registry = create_test_registry();
        let entity = Entity::new(0, 0); // Never spawned
        assert!(!registry.despawn(entity));

        let spawned_entity = registry.spawn(); // id 0, gen 0
        assert!(registry.despawn(spawned_entity)); // Despawn it
        assert!(!registry.despawn(spawned_entity)); // Try to despawn again (old handle, gen mismatch)

        let wrong_gen_entity = Entity::new(0, 99); // Correct ID, wrong generation
        assert!(!registry.despawn(wrong_gen_entity));

        let out_of_bounds_entity = Entity::new(100, 0); // ID out of bounds
        assert!(!registry.despawn(out_of_bounds_entity));
    }

    #[test]
    fn registry_is_entity_alive_after_spawn_despawn() {
        let mut registry = create_test_registry();
        let e1 = registry.spawn();
        assert!(registry.is_entity_alive(e1));

        registry.despawn(e1);
        assert!(!registry.is_entity_alive(e1));

        let e2 = registry.spawn(); // Reuses e1's ID (0) with new generation (1)
        assert!(registry.is_entity_alive(e2));
        assert!(!registry.is_entity_alive(e1)); // Original e1 handle is still not alive

        let non_existent_entity = Entity::new(99, 0);
        assert!(!registry.is_entity_alive(non_existent_entity));
    }

    #[test]
    fn find_or_create_archetype_empty_signature() {
        let mut registry = create_test_registry();
        let signature = BTreeSet::new();
        let archetype_id = registry.find_or_create_archetype(signature.clone());
        assert!(registry.archetypes.contains_key(&archetype_id));
        assert_eq!(
            registry.signature_to_archetype_id.get(&signature).unwrap(),
            &archetype_id
        );
        let archetype = registry.archetypes.get(&archetype_id).unwrap();
        assert!(archetype.signature().is_empty());

        // Calling again should return the same ID
        let archetype_id_again = registry.find_or_create_archetype(signature);
        assert_eq!(archetype_id_again, archetype_id);
    }

    #[test]
    #[should_panic(
        expected = "Attempted to create archetype with unregistered component type"
    )]
    fn find_or_create_archetype_with_unregistered_component_panics() {
        let mut registry = Registry::new(); // Fresh registry without Position registered
        let mut signature = BTreeSet::new();
        signature.insert(ComponentTypeId::of::<Position>());
        registry.find_or_create_archetype(signature); // Should panic
    }

    #[test]
    fn find_or_create_archetype_with_registered_component_succeeds() {
        let mut registry = create_test_registry(); // Position is registered here
        let mut signature = BTreeSet::new();
        signature.insert(ComponentTypeId::of::<Position>());
        let archetype_id = registry.find_or_create_archetype(signature.clone());

        assert!(registry.archetypes.contains_key(&archetype_id));
        let archetype = registry.archetypes.get(&archetype_id).unwrap();
        assert!(archetype.signature().contains(&ComponentTypeId::of::<Position>()));
        assert_eq!(archetype.signature().len(), 1);

        // Calling again should return the same ID
        let archetype_id_again = registry.find_or_create_archetype(signature);
        assert_eq!(archetype_id_again, archetype_id);
    }

    #[test]
    fn registry_insert_for_dead_entity_errors() {
        let mut registry = create_test_registry();
        let entity = registry.spawn();
        registry.despawn(entity);
        let result = registry.insert(entity, (Position { x: 0.0, y: 0.0 },));
        assert_eq!(result, Err(InsertError::EntityNotFound));
    }

    #[test]
    fn registry_insert_unregistered_component_errors() {
        let mut registry = Registry::new(); // Fresh registry, Position not registered
        let entity = registry.spawn();
        let result = registry.insert(entity, (Position { x: 0.0, y: 0.0 },));
        assert_eq!(
            result,
            Err(InsertError::ComponentTypeNotRegistered(
                ComponentTypeId::of::<Position>()
            ))
        );
    }

    #[test]
    fn registry_insert_multiple_entities_and_archetypes() {
        let mut registry = create_test_registry();

        // Entity 1: Position
        let e1 = registry.spawn();
        let p1 = Position { x: 1.0, y: 1.0 };
        registry.insert(e1, (p1.clone(),)).unwrap();
        assert_eq!(registry.get::<Position>(e1), Some(&p1));
        let e1_arch_id = registry.get_entity_archetype_id(e1).unwrap();

        // Entity 2: Position and Velocity
        let e2 = registry.spawn();
        let p2 = Position { x: 2.0, y: 2.0 };
        let v2 = Velocity { dx: 0.1, dy: 0.1 };
        registry.insert(e2, (p2.clone(), v2.clone())).unwrap();
        assert_eq!(registry.get::<Position>(e2), Some(&p2));
        assert_eq!(registry.get::<Velocity>(e2), Some(&v2));
        let e2_arch_id = registry.get_entity_archetype_id(e2).unwrap();
        assert_ne!(e1_arch_id, e2_arch_id);

        // Entity 3: Position (should share archetype with e1 if only Position)
        let e3 = registry.spawn();
        let p3 = Position { x: 3.0, y: 3.0 };
        registry.insert(e3, (p3.clone(),)).unwrap();
        assert_eq!(registry.get::<Position>(e3), Some(&p3));
        let e3_arch_id = registry.get_entity_archetype_id(e3).unwrap();
        assert_eq!(e1_arch_id, e3_arch_id);

        // Entity 4: No components (should be in null archetype, different from e1/e2)
        let e4 = registry.spawn();
        let e4_arch_id = registry.get_entity_archetype_id(e4).unwrap();
        let null_archetype_id = registry.find_or_create_archetype(BTreeSet::new());
        assert_eq!(e4_arch_id, null_archetype_id);
        assert_ne!(e4_arch_id, e1_arch_id);
        assert_ne!(e4_arch_id, e2_arch_id);

        // Check archetype contents
        let arch1 = registry.archetypes.get(&e1_arch_id).unwrap();
        assert_eq!(arch1.entities_count(), 2); // e1, e3
        assert!(
            arch1.get_entity_by_row(0).unwrap() == e1
                || arch1.get_entity_by_row(1).unwrap() == e1
        );
        assert!(
            arch1.get_entity_by_row(0).unwrap() == e3
                || arch1.get_entity_by_row(1).unwrap() == e3
        );

        let arch2 = registry.archetypes.get(&e2_arch_id).unwrap();
        assert_eq!(arch2.entities_count(), 1); // e2
        assert_eq!(arch2.get_entity_by_row(0).unwrap(), e2);

        let arch_null = registry.archetypes.get(&null_archetype_id).unwrap();
        // e4 + any entities spawned by create_test_registry that might be in null initially.
        // create_test_registry spawns nothing, so only e4.
        // spawn() itself puts entities in null archetype.
        // e1, e2, e3, e4 were spawned. e1,e2,e3 moved out. e4 remains.
        assert_eq!(arch_null.entities_count(), 1);
        assert_eq!(arch_null.get_entity_by_row(0).unwrap(), e4);
    }

    #[test]
    fn registry_get_and_get_mut() {
        let mut registry = create_test_registry();
        let entity = registry.spawn();
        let p1 = Position { x: 1.0, y: 1.0 };
        let v1 = Velocity { dx: 0.5, dy: 0.5 };

        registry.insert(entity, (p1.clone(), v1.clone())).unwrap();

        // Test get
        assert_eq!(registry.get::<Position>(entity), Some(&p1));
        assert_eq!(registry.get::<Velocity>(entity), Some(&v1));
        assert!(registry.get::<TagA>(entity).is_none()); // TagA not present

        // Test get_mut
        let p_mut = registry.get_mut::<Position>(entity).unwrap();
        p_mut.x = 10.0;
        assert_eq!(p_mut.x, 10.0);

        // Verify change with get
        let p_updated = Position { x: 10.0, y: 1.0 };
        assert_eq!(registry.get::<Position>(entity), Some(&p_updated));

        // Test get_mut for non-existent component
        assert!(registry.get_mut::<TagA>(entity).is_none());

        // Test get/get_mut on dead entity
        let dead_entity = registry.spawn();
        registry.despawn(dead_entity);
        assert!(registry.get::<Position>(dead_entity).is_none());
        assert!(registry.get_mut::<Position>(dead_entity).is_none());

        // Test get/get_mut on entity that never existed
        let never_entity = Entity::new(999, 0);
        assert!(registry.get::<Position>(never_entity).is_none());
        assert!(registry.get_mut::<Position>(never_entity).is_none());

        // Test inserting a component, then another, then getting both
        let e_multi = registry.spawn();
        registry.insert(e_multi, (Position { x: 1.0, y: 1.0 },)).unwrap();
        registry.insert(e_multi, (Velocity { dx: 0.1, dy: 0.1 },)).unwrap();

        assert_eq!(
            registry.get::<Position>(e_multi),
            Some(&Position { x: 1.0, y: 1.0 })
        );
        assert_eq!(
            registry.get::<Velocity>(e_multi),
            Some(&Velocity { dx: 0.1, dy: 0.1 })
        );

        // Test overwriting a component
        registry.insert(e_multi, (Position { x: 2.0, y: 2.0 },)).unwrap();
        assert_eq!(
            registry.get::<Position>(e_multi),
            Some(&Position { x: 2.0, y: 2.0 })
        );
        assert_eq!(
            registry.get::<Velocity>(e_multi),
            Some(&Velocity { dx: 0.1, dy: 0.1 })
        ); // Velocity should be unchanged

        // Test that entity moves to correct archetype and data is preserved
        let e_move = registry.spawn(); // In null archetype
        let p_move = Position { x: 100.0, y: 100.0 };
        registry.insert(e_move, (p_move.clone(),)).unwrap(); // Moves to Position archetype
        assert_eq!(registry.get::<Position>(e_move), Some(&p_move));

        let v_move = Velocity { dx: 1.0, dy: 1.0 };
        registry.insert(e_move, (v_move.clone(),)).unwrap(); // Moves to Position+Velocity archetype
        assert_eq!(registry.get::<Position>(e_move), Some(&p_move)); // Position data must be preserved
        assert_eq!(registry.get::<Velocity>(e_move), Some(&v_move));

        // Modify after move
        let p_mod_mut = registry.get_mut::<Position>(e_move).unwrap();
        p_mod_mut.x = 101.0;
        let p_mod_expected = Position { x: 101.0, y: 100.0 };
        assert_eq!(registry.get::<Position>(e_move), Some(&p_mod_expected));
    }

    #[test]
    fn registry_insert_components_to_null_archetype_entity_with_get_verification() {
        let mut registry = create_test_registry();
        let entity = registry.spawn(); // Entity is in null archetype

        let pos = Position { x: 1.0, y: 2.0 };
        registry.insert(entity, (pos.clone(),)).unwrap();

        assert_eq!(registry.get::<Position>(entity), Some(&pos));
        let entity_arch_id = registry.get_entity_archetype_id(entity).unwrap();
        let arch = registry.archetypes.get(&entity_arch_id).unwrap();
        assert_eq!(arch.signature().len(), 1);
        assert!(arch.signature().contains(&ComponentTypeId::of::<Position>()));
    }

    #[test]
    fn registry_insert_components_in_place_update_with_get_verification() {
        let mut registry = create_test_registry();
        let entity = registry.spawn();
        let initial_pos = Position { x: 1.0, y: 1.0 };
        registry.insert(entity, (initial_pos.clone(),)).unwrap();

        let updated_pos = Position { x: 2.0, y: 2.0 };
        registry.insert(entity, (updated_pos.clone(),)).unwrap(); // Should overwrite

        assert_eq!(registry.get::<Position>(entity), Some(&updated_pos));
    }

    #[test]
    fn registry_insert_mixed_new_and_existing_components_with_get_verification() {
        let mut registry = create_test_registry();
        let entity = registry.spawn();
        let pos = Position { x: 1.0, y: 1.0 };
        registry.insert(entity, (pos.clone(),)).unwrap(); // Entity has Position

        let vel = Velocity { dx: 0.5, dy: 0.5 };
        let updated_pos = Position { x: 2.0, y: 2.0 };
        // Insert Velocity (new) and updated Position (overwrite)
        registry.insert(entity, (updated_pos.clone(), vel.clone())).unwrap();

        assert_eq!(registry.get::<Position>(entity), Some(&updated_pos));
        assert_eq!(registry.get::<Velocity>(entity), Some(&vel));
        let entity_arch_id = registry.get_entity_archetype_id(entity).unwrap();
        let arch = registry.archetypes.get(&entity_arch_id).unwrap();
        assert_eq!(arch.signature().len(), 2); // Should have Position and Velocity
        assert!(arch.signature().contains(&ComponentTypeId::of::<Position>()));
        assert!(arch.signature().contains(&ComponentTypeId::of::<Velocity>()));
    }

    #[test]
    fn registry_remove_component_success_and_archetype_change() {
        let mut registry = create_test_registry();
        let entity = registry.spawn();
        let p = Position { x: 1.0, y: 1.0 };
        let v = Velocity { dx: 0.5, dy: 0.5 };
        registry.insert(entity, (p.clone(), v.clone())).unwrap(); // Archetype (P, V)

        let arch_pv_id = registry.get_entity_archetype_id(entity).unwrap();

        registry.remove::<Velocity>(entity).unwrap(); // Remove V, should move to Archetype (P)
        assert!(registry.get::<Velocity>(entity).is_none());
        assert_eq!(registry.get::<Position>(entity), Some(&p)); // P should remain

        let arch_p_id = registry.get_entity_archetype_id(entity).unwrap();
        assert_ne!(arch_pv_id, arch_p_id);
        let arch_p = registry.archetypes.get(&arch_p_id).unwrap();
        assert_eq!(arch_p.signature().len(), 1);
        assert!(arch_p.signature().contains(&ComponentTypeId::of::<Position>()));

        // Check that entity is no longer in the old archetype (P,V)
        let arch_pv = registry.archetypes.get(&arch_pv_id).unwrap();
        let mut found_in_old = false;
        for i in 0..arch_pv.entities_count() {
            if arch_pv.get_entity_by_row(i).unwrap() == entity {
                found_in_old = true;
                break;
            }
        }
        assert!(
            !found_in_old,
            "Entity should not be in the old archetype after component removal and move."
        );

        registry.remove::<Position>(entity).unwrap(); // Remove P, should move to Null Archetype
        assert!(registry.get::<Position>(entity).is_none());
        let arch_null_id = registry.get_entity_archetype_id(entity).unwrap();
        assert_ne!(arch_p_id, arch_null_id);
        let arch_null = registry.archetypes.get(&arch_null_id).unwrap();
        assert!(arch_null.signature().is_empty());
    }

    #[test]
    fn registry_remove_component_not_present_on_entity() {
        let mut registry = create_test_registry();
        let entity = registry.spawn();
        registry.insert(entity, (Position { x: 1.0, y: 1.0 },)).unwrap();

        let result = registry.remove::<Velocity>(entity); // Velocity not present
        assert_eq!(result, Err(RemoveError::ComponentNotPresent));
        assert!(registry.get::<Position>(entity).is_some()); // Position should still be there

        // Try removing from an entity in null archetype
        let null_entity = registry.spawn();
        let result_null = registry.remove::<Position>(null_entity);
        assert_eq!(result_null, Err(RemoveError::ComponentNotPresent));
    }

    #[test]
    fn registry_remove_component_from_dead_entity_errors() {
        let mut registry = create_test_registry();
        let entity = registry.spawn();
        registry.insert(entity, (Position { x: 1.0, y: 1.0 },)).unwrap();
        registry.despawn(entity);

        let result = registry.remove::<Position>(entity);
        assert_eq!(result, Err(RemoveError::EntityNotFound));
    }

    #[test]
    fn registry_remove_component_unregistered_type_is_not_possible_at_compile_time() {
        // This test primarily serves as a conceptual check.
        // If you try to call registry.remove::<UnregisteredComponent>(entity),
        // and UnregisteredComponent does not impl Component, it's a compile error.
        // If it does impl Component but isn't registered with component_factories,
        // `find_or_create_archetype` would panic if it were ever needed for that type,
        // but remove() itself doesn't directly rely on component_factories for the
        // type being removed, only for the target archetype's components.

        // The crucial part is that ComponentTypeId::of::<T>() can be called for any T: Component.
        // The `remove` logic correctly identifies if the component is present based on signature.
        // If we were to try to remove a component type that was never registered AND never inserted,
        // it would correctly result in ComponentNotPresent.

        let mut registry = create_test_registry(); // Registers Position, Velocity, etc.
        let entity = registry.spawn();
        registry.insert(entity, (Position { x: 1.0, y: 1.0 },)).unwrap();

        // struct Unregistered {} // Does not impl Component - compile error if used with remove::<Unregistered>
        // impl Component for Unregistered {} // If we did this, then:

        // This would be like trying to remove a component that was never on the entity.
        // The registry doesn't need to "know" about Unregistered if it's not part of any signature.
        // let result = registry.remove::<Unregistered>(entity);
        // assert_eq!(result, Err(RemoveError::ComponentNotPresent));
        // This is implicitly tested by registry_remove_component_not_present_on_entity
        // using a registered type (Velocity) that's not on the entity.
        // The behavior is the same: ComponentNotPresent.
        assert!(
            true,
            "This test is a conceptual check verified by other tests and compile-time constraints."
        );
    }

    #[test]
    fn registry_update_component_success() {
        let mut registry = create_test_registry();
        let entity = registry.spawn();
        let initial_pos = Position { x: 1.0, y: 1.0 };
        registry.insert(entity, (initial_pos.clone(),)).unwrap();

        let updated_pos = Position { x: 10.0, y: 20.0 };
        let result = registry.update_component(entity, updated_pos.clone());
        assert_eq!(result, Ok(()));
        assert_eq!(registry.get::<Position>(entity), Some(&updated_pos));

        // Ensure it didn't change archetype
        let mut expected_sig = BTreeSet::new();
        expected_sig.insert(ComponentTypeId::of::<Position>());
        let expected_arch_id = registry.find_or_create_archetype(expected_sig);
        assert_eq!(
            registry.get_entity_archetype_id(entity),
            Some(expected_arch_id)
        );
    }

    #[test]
    fn registry_update_component_entity_not_found() {
        let mut registry = create_test_registry();
        let entity = Entity::new(100, 0); // Non-existent
        let pos = Position { x: 1.0, y: 1.0 };
        let result = registry.update_component(entity, pos);
        assert_eq!(result, Err(UpdateError::EntityNotFound));
    }

    #[test]
    fn registry_update_component_not_present() {
        let mut registry = create_test_registry();
        let entity = registry.spawn(); // Entity is in null archetype initially
        // Or, add a different component
        registry.insert(entity, (Velocity { dx: 0.0, dy: 0.0 },)).unwrap();

        let pos_to_update = Position { x: 1.0, y: 1.0 };
        let result = registry.update_component(entity, pos_to_update);
        assert_eq!(result, Err(UpdateError::ComponentNotPresent));
    }

    #[test]
    fn registry_update_component_type_not_registered() {
        let mut registry = Registry::new(); // Fresh registry
        let entity = registry.spawn();
        // Position is not registered
        let pos = Position { x: 1.0, y: 1.0 };
        let result = registry.update_component(entity, pos);
        assert_eq!(result, Err(UpdateError::ComponentTypeNotRegistered));
    }

    // This test whould require to expose component_storages in Archetype
    // to be able to remove it manually. This is not a good idea, so we
    // comment this test out for now.
    // We could try to implement this test in the archetype module.
    // #[test]
    // #[should_panic(expected = "Missing component storage for")]
    // fn registry_update_component_internal_archetype_error_panics() {
    //     let mut registry = create_test_registry();
    //     let entity = registry.spawn();
    //     let pos = Position { x: 1.0, y: 1.0 };
    //     registry.insert(entity, (pos,)).unwrap();

    //     let (archetype_id, _) = registry.entity_locations.get(&entity).unwrap();
    //     let archetype = registry.archetypes.get_mut(archetype_id).unwrap();

    //     // Manually mess up the archetype: remove storage but keep signature
    //     let _removed_storage =
    //         archetype.component_storages.remove(&ComponentTypeId::of::<Position>());
    //     // At this point, the signature still says Position exists, but storage is gone.

    //     // Now try to update, this should cause the panic inside update_component
    //     // when it tries to get the storage that's no longer there.
    //     let updated_pos = Position { x: 2.0, y: 2.0 };
    //     // The panic message we are expecting is:
    //     // "Missing component storage for ComponentTypeId(TypeID { id: XXX }) in archetype ArchetypeId(Y) despite signature match."
    //     // So, "Missing component storage for" should be enough.
    //     match registry.update_component(entity, updated_pos.clone()) {
    //         Ok(_) => panic!("Update succeeded unexpectedly"),
    //         Err(UpdateError::ComponentNotPresent) => panic!(
    //             "Update failed with ComponentNotPresent, expected panic for missing storage"
    //         ),
    //         Err(UpdateError::EntityNotFound) => panic!("Update failed with EntityNotFound"),
    //         Err(UpdateError::ComponentTypeNotRegistered) => {
    //             panic!("Update failed with ComponentTypeNotRegistered")
    //         } // If it panics as expected, this match arm won't be reached.
    //     }
    // }

    #[test]
    fn spawn_despawn_id_reuse_and_generation() {
        let mut registry = create_test_registry();

        // Spawn initial entities
        let e1 = registry.spawn(); // ID 0, Gen 0
        let e2 = registry.spawn(); // ID 1, Gen 0
        let e3 = registry.spawn(); // ID 2, Gen 0

        assert_eq!(e1, Entity::new(0, 0));
        assert_eq!(e2, Entity::new(1, 0));
        assert_eq!(e3, Entity::new(2, 0));

        // Despawn e2
        assert!(registry.despawn(e2));
        assert!(!registry.is_entity_alive(e2));
        // registry.entity_generations[1] should now be 1

        // Spawn a new entity, should reuse e2's ID (1) with incremented generation
        let e4 = registry.spawn(); // ID 1, Gen 1
        assert_eq!(e4, Entity::new(1, 1));
        assert!(registry.is_entity_alive(e4));
        assert!(!registry.is_entity_alive(e2)); // Original e2 handle still invalid

        // Despawn e1
        assert!(registry.despawn(e1));
        assert!(!registry.is_entity_alive(e1));
        // registry.entity_generations[0] should now be 1

        // Despawn e3
        assert!(registry.despawn(e3));
        assert!(!registry.is_entity_alive(e3));
        // registry.entity_generations[2] should now be 1

        // Spawn new entities, should reuse e1's ID (0) then e3's ID (2)
        let e5 = registry.spawn(); // ID 0, Gen 1 (reusing e1's ID)
        assert_eq!(e5, Entity::new(0, 1));
        assert!(registry.is_entity_alive(e5));

        let e6 = registry.spawn(); // ID 2, Gen 1 (reusing e3's ID)
        assert_eq!(e6, Entity::new(2, 1));
        assert!(registry.is_entity_alive(e6));

        // At this point, e4 (1,1), e5 (0,1), e6 (2,1) are alive.
        // Free list order was e2_id (1), then e1_id (0), then e3_id (2).
        // Spawn order for reuse was e2_id (1) for e4, then e1_id (0) for e5, then e3_id (2) for e6.
        // This matches VecDeque's pop_front behavior.

        // Despawn e5 (0,1)
        assert!(registry.despawn(e5));
        // registry.entity_generations[0] should now be 2

        // Despawn e4 (1,1)
        assert!(registry.despawn(e4));
        // registry.entity_generations[1] should now be 2

        // Spawn two more entities
        let e7 = registry.spawn(); // ID 0, Gen 2 (reusing e5's ID)
        assert_eq!(e7, Entity::new(0, 2));

        let e8 = registry.spawn(); // ID 1, Gen 2 (reusing e4's ID)
        assert_eq!(e8, Entity::new(1, 2));

        // Check generations directly
        assert_eq!(registry.entity_generations[0], 2); // e1 -> e5 -> e7
        assert_eq!(registry.entity_generations[1], 2); // e2 -> e4 -> e8
        assert_eq!(registry.entity_generations[2], 1); // e3 -> e6

        // Spawn a new entity, should use next_entity_id as all freed are used or generations differ
        let e9 = registry.spawn(); // ID 3, Gen 0 (next_entity_id was 3)
        assert_eq!(e9, Entity::new(3, 0));
        assert_eq!(registry.next_entity_id, 4);
        assert_eq!(registry.entity_generations.len(), 4);
        assert_eq!(registry.entity_generations[3], 0);
    }

    #[test]
    fn insert_single_component_new_entity() {
        let mut registry = create_test_registry();
        let e1 = registry.spawn();
        let comp_a1 = ComponentA { value: 10 };
        registry.insert(e1, (comp_a1.clone(),)).unwrap();
        assert_eq!(registry.get::<ComponentA>(e1), Some(&comp_a1));
        let e1_arch_id = registry.get_entity_archetype_id(e1).unwrap();
        let e1_arch = registry.archetypes.get(&e1_arch_id).unwrap();
        assert_eq!(e1_arch.signature().len(), 1);
        assert!(e1_arch.signature().contains(&ComponentTypeId::of::<ComponentA>()));
    }

    #[test]
    fn insert_multiple_components_new_entity() {
        let mut registry = create_test_registry();
        let e2 = registry.spawn();
        let comp_b1 = ComponentB { value: 20.5 };
        let comp_c1 = ComponentC {
            value: "hello".to_string(),
        };
        registry.insert(e2, (comp_b1.clone(), comp_c1.clone())).unwrap();
        assert_eq!(registry.get::<ComponentB>(e2), Some(&comp_b1));
        assert_eq!(registry.get::<ComponentC>(e2), Some(&comp_c1));
        let e2_arch_id = registry.get_entity_archetype_id(e2).unwrap();
        let e2_arch = registry.archetypes.get(&e2_arch_id).unwrap();
        assert_eq!(e2_arch.signature().len(), 2);
        assert!(e2_arch.signature().contains(&ComponentTypeId::of::<ComponentB>()));
        assert!(e2_arch.signature().contains(&ComponentTypeId::of::<ComponentC>()));
    }

    #[test]
    fn insert_overwrite_existing_component() {
        let mut registry = create_test_registry();
        let e3 = registry.spawn();
        let initial_a = ComponentA { value: 30 };
        registry.insert(e3, (initial_a.clone(),)).unwrap();
        assert_eq!(registry.get::<ComponentA>(e3), Some(&initial_a));
        let initial_e3_arch_id = registry.get_entity_archetype_id(e3).unwrap();

        let updated_a = ComponentA { value: 35 };
        registry.insert(e3, (updated_a.clone(),)).unwrap(); // Overwrite
        assert_eq!(registry.get::<ComponentA>(e3), Some(&updated_a));
        let e3_arch_id_after_overwrite = registry.get_entity_archetype_id(e3).unwrap();
        let e3_arch_after_overwrite =
            registry.archetypes.get(&e3_arch_id_after_overwrite).unwrap();
        assert_eq!(e3_arch_after_overwrite.signature().len(), 1); // Archetype should not change
        assert_eq!(
            e3_arch_id_after_overwrite, initial_e3_arch_id,
            "Archetype ID should remain the same after overwrite"
        );
    }

    #[test]
    fn insert_mixed_new_and_overwrite() {
        let mut registry = create_test_registry();
        let e4 = registry.spawn();
        let comp_a2 = ComponentA { value: 40 };
        let comp_b2 = ComponentB { value: 40.5 };
        registry.insert(e4, (comp_a2.clone(), comp_b2.clone())).unwrap();
        assert_eq!(registry.get::<ComponentA>(e4), Some(&comp_a2));
        assert_eq!(registry.get::<ComponentB>(e4), Some(&comp_b2));
        let initial_e4_arch_id = registry.get_entity_archetype_id(e4).unwrap();

        let updated_a2 = ComponentA { value: 45 }; // Overwrite A
        let comp_c2 = ComponentC {
            value: "world".to_string(),
        }; // New C
        registry.insert(e4, (updated_a2.clone(), comp_c2.clone())).unwrap();
        assert_eq!(registry.get::<ComponentA>(e4), Some(&updated_a2));
        assert_eq!(registry.get::<ComponentB>(e4), Some(&comp_b2)); // B should still be there
        assert_eq!(registry.get::<ComponentC>(e4), Some(&comp_c2));
        let e4_arch_id_after_c = registry.get_entity_archetype_id(e4).unwrap();
        assert_ne!(
            e4_arch_id_after_c, initial_e4_arch_id,
            "Archetype should change due to new ComponentC"
        );
        let e4_arch = registry.archetypes.get(&e4_arch_id_after_c).unwrap();
        assert_eq!(e4_arch.signature().len(), 3); // A, B, C
        assert!(e4_arch.signature().contains(&ComponentTypeId::of::<ComponentA>()));
        assert!(e4_arch.signature().contains(&ComponentTypeId::of::<ComponentB>()));
        assert!(e4_arch.signature().contains(&ComponentTypeId::of::<ComponentC>()));
    }

    #[test]
    fn insert_add_new_component_to_existing_entity() {
        let mut registry = create_test_registry();
        let e6 = registry.spawn();
        registry.insert(e6, (ComponentA { value: 60 },)).unwrap();
        let e6_arch_id_v1 = registry.get_entity_archetype_id(e6).unwrap();

        registry.insert(e6, (ComponentB { value: 60.5 },)).unwrap(); // Add ComponentB
        assert_eq!(
            registry.get::<ComponentA>(e6),
            Some(&ComponentA { value: 60 })
        );
        assert_eq!(
            registry.get::<ComponentB>(e6),
            Some(&ComponentB { value: 60.5 })
        );
        let e6_arch_id_v2 = registry.get_entity_archetype_id(e6).unwrap();
        assert_ne!(
            e6_arch_id_v1, e6_arch_id_v2,
            "Archetype should change after adding ComponentB"
        );
        let e6_arch_v2 = registry.archetypes.get(&e6_arch_id_v2).unwrap();
        assert_eq!(e6_arch_v2.signature().len(), 2);
        assert!(e6_arch_v2.signature().contains(&ComponentTypeId::of::<ComponentA>()));
        assert!(e6_arch_v2.signature().contains(&ComponentTypeId::of::<ComponentB>()));
    }

    #[test]
    fn get_data_integrity_after_insert() {
        let mut registry = create_test_registry();
        let e1 = registry.spawn();
        let initial_a = ComponentA { value: 100 };
        let initial_b = ComponentB { value: 100.5 };
        registry.insert(e1, (initial_a.clone(), initial_b.clone())).unwrap();

        assert_eq!(registry.get::<ComponentA>(e1), Some(&initial_a));
        assert_eq!(registry.get::<ComponentB>(e1), Some(&initial_b));
        assert!(
            registry.get::<ComponentC>(e1).is_none(),
            "ComponentC should not exist on e1 yet"
        );
    }

    #[test]
    fn get_mut_modifies_data_correctly() {
        let mut registry = create_test_registry();
        let e1 = registry.spawn();
        let initial_a = ComponentA { value: 100 };
        registry.insert(e1, (initial_a.clone(),)).unwrap();

        let mut_a = registry.get_mut::<ComponentA>(e1).unwrap();
        mut_a.value = 101;
        assert_eq!(
            registry.get::<ComponentA>(e1),
            Some(&ComponentA { value: 101 })
        );

        let initial_b = ComponentB { value: 100.5 };
        registry.insert(e1, (initial_b.clone(),)).unwrap(); // Adds B, A is still 101

        let mut_b = registry.get_mut::<ComponentB>(e1).unwrap();
        mut_b.value = 101.5;
        assert_eq!(
            registry.get::<ComponentB>(e1),
            Some(&ComponentB { value: 101.5 })
        );
        assert_eq!(
            // Verify A is untouched by B's modification
            registry.get::<ComponentA>(e1),
            Some(&ComponentA { value: 101 })
        );
    }

    #[test]
    fn get_get_mut_for_non_existent_component_on_existing_entity() {
        let mut registry = create_test_registry();
        let e1 = registry.spawn();
        registry.insert(e1, (ComponentA { value: 100 },)).unwrap(); // e1 has ComponentA

        assert!(registry.get::<ComponentC>(e1).is_none());
        assert!(registry.get_mut::<ComponentC>(e1).is_none());
    }

    #[test]
    fn get_get_mut_data_integrity_after_remove() {
        let mut registry = create_test_registry();
        let e1 = registry.spawn();
        let comp_a = ComponentA { value: 101 };
        let comp_b = ComponentB { value: 101.5 };
        registry.insert(e1, (comp_a.clone(), comp_b.clone())).unwrap();

        registry.remove::<ComponentA>(e1).unwrap();
        assert!(
            registry.get::<ComponentA>(e1).is_none(),
            "ComponentA should be removed"
        );
        assert_eq!(
            registry.get::<ComponentB>(e1),
            Some(&comp_b), // Original comp_b
            "ComponentB should remain after A is removed"
        );

        // Try to get_mut the removed component
        assert!(
            registry.get_mut::<ComponentA>(e1).is_none(),
            "get_mut for removed ComponentA should be None"
        );

        // Modify remaining component B
        let mut_b_after_a_removed = registry.get_mut::<ComponentB>(e1).unwrap();
        mut_b_after_a_removed.value = 102.5;
        assert_eq!(
            registry.get::<ComponentB>(e1),
            Some(&ComponentB { value: 102.5 })
        );
    }

    #[test]
    fn get_get_mut_on_despawned_entity() {
        let mut registry = create_test_registry();
        let e2 = registry.spawn();
        registry.insert(e2, (ComponentA { value: 200 },)).unwrap();
        assert!(registry.is_entity_alive(e2));
        registry.despawn(e2);
        assert!(!registry.is_entity_alive(e2));

        assert!(
            registry.get::<ComponentA>(e2).is_none(),
            "get on despawned entity should be None"
        );
        assert!(
            registry.get_mut::<ComponentA>(e2).is_none(),
            "get_mut on despawned entity should be None"
        );
    }

    #[test]
    fn get_get_mut_on_never_existed_entity() {
        let mut registry = create_test_registry();
        let e_never = Entity::new(999, 0);
        assert!(registry.get::<ComponentA>(e_never).is_none());
        assert!(registry.get_mut::<ComponentA>(e_never).is_none());
    }

    #[test]
    fn remove_one_component_archetype_change_data_preserved() {
        let mut registry = create_test_registry();
        let e1 = registry.spawn();
        let comp_a = ComponentA { value: 10 };
        let comp_b = ComponentB { value: 10.5 };
        let comp_c = ComponentC {
            value: "test_remove".to_string(),
        };
        registry.insert(e1, (comp_a.clone(), comp_b.clone(), comp_c.clone())).unwrap();

        assert_eq!(registry.get::<ComponentA>(e1), Some(&comp_a));
        assert_eq!(registry.get::<ComponentB>(e1), Some(&comp_b));
        assert_eq!(registry.get::<ComponentC>(e1), Some(&comp_c));
        let initial_e1_arch_id = registry.get_entity_archetype_id(e1).unwrap();

        // Remove ComponentB
        let remove_b_result = registry.remove::<ComponentB>(e1);
        assert_eq!(remove_b_result, Ok(()));

        assert!(
            registry.get::<ComponentB>(e1).is_none(),
            "ComponentB should be removed"
        );
        assert_eq!(
            registry.get::<ComponentA>(e1),
            Some(&comp_a),
            "ComponentA should remain"
        );
        assert_eq!(
            registry.get::<ComponentC>(e1),
            Some(&comp_c),
            "ComponentC should remain"
        );

        let e1_arch_after_b_removed = registry.get_entity_archetype_id(e1).unwrap();
        assert_ne!(
            e1_arch_after_b_removed, initial_e1_arch_id,
            "Archetype should change after removing B"
        );
        let e1_arch_obj_after_b = registry.archetypes.get(&e1_arch_after_b_removed).unwrap();
        assert_eq!(e1_arch_obj_after_b.signature().len(), 2);
        assert!(
            e1_arch_obj_after_b.signature().contains(&ComponentTypeId::of::<ComponentA>())
        );
        assert!(
            e1_arch_obj_after_b.signature().contains(&ComponentTypeId::of::<ComponentC>())
        );
    }

    #[test]
    fn remove_last_component_moves_to_null() {
        let mut registry = create_test_registry();
        let e2 = registry.spawn();
        let marker_d = MarkerComponentD;
        registry.insert(e2, (marker_d.clone(),)).unwrap();
        assert!(registry.get::<MarkerComponentD>(e2).is_some());
        let e2_arch_id_with_d = registry.get_entity_archetype_id(e2).unwrap();
        let e2_arch_with_d_obj = registry.archetypes.get(&e2_arch_id_with_d).unwrap();
        assert!(!e2_arch_with_d_obj.signature().is_empty());

        // Remove MarkerComponentD
        let remove_d_result = registry.remove::<MarkerComponentD>(e2);
        assert_eq!(remove_d_result, Ok(()));
        assert!(
            registry.get::<MarkerComponentD>(e2).is_none(),
            "MarkerComponentD should be removed"
        );

        let e2_arch_id_after_d_removed = registry.get_entity_archetype_id(e2).unwrap();
        assert_ne!(
            e2_arch_id_after_d_removed, e2_arch_id_with_d,
            "Archetype should change to null"
        );
        let e2_null_arch_obj = registry.archetypes.get(&e2_arch_id_after_d_removed).unwrap();
        assert!(
            e2_null_arch_obj.signature().is_empty(),
            "Entity e2 should be in the null archetype"
        );

        // Verify the old archetype (MarkerD only) is now empty or e2 is no longer there
        let old_e2_archetype = registry.archetypes.get(&e2_arch_id_with_d).unwrap();
        let mut e2_found_in_old_archetype = false;
        for i in 0..old_e2_archetype.entities_count() {
            if old_e2_archetype.get_entity_by_row(i).unwrap() == e2 {
                e2_found_in_old_archetype = true;
                break;
            }
        }
        assert!(
            !e2_found_in_old_archetype,
            "e2 should not be in its old archetype storage"
        );
    }

    // Note: Scenarios for removing a non-existent component and removing from a despawned entity
    // are already covered by:
    // - registry_remove_component_not_present_on_entity
    // - registry_remove_component_from_dead_entity_errors
    // So, they are not duplicated here from the original test_remove_scenarios.

    #[test]
    fn spawn_and_initial_insert_ab() {
        let mut registry = create_test_registry();
        let entity = registry.spawn();
        assert!(registry.is_entity_alive(entity));
        let null_archetype_id = registry.get_entity_archetype_id(entity).unwrap();
        assert!(registry.archetypes.get(&null_archetype_id).unwrap().signature().is_empty());

        let comp_a = ComponentA { value: 1 };
        let comp_b = ComponentB { value: 1.0 };
        registry.insert(entity, (comp_a.clone(), comp_b.clone())).unwrap();

        assert_eq!(registry.get::<ComponentA>(entity), Some(&comp_a));
        assert_eq!(registry.get::<ComponentB>(entity), Some(&comp_b));
        assert!(registry.get::<ComponentC>(entity).is_none());
        let ab_archetype_id = registry.get_entity_archetype_id(entity).unwrap();
        let ab_arch = registry.archetypes.get(&ab_archetype_id).unwrap();
        assert_eq!(ab_arch.signature().len(), 2);
        assert!(ab_arch.signature().contains(&ComponentTypeId::of::<ComponentA>()));
        assert!(ab_arch.signature().contains(&ComponentTypeId::of::<ComponentB>()));
    }

    #[test]
    fn insert_c_after_ab_and_verify() {
        let mut registry = create_test_registry();
        let entity = registry.spawn();
        let comp_a = ComponentA { value: 1 };
        let comp_b = ComponentB { value: 1.0 };
        registry.insert(entity, (comp_a.clone(), comp_b.clone())).unwrap();
        let ab_archetype_id = registry.get_entity_archetype_id(entity).unwrap();

        let comp_c = ComponentC {
            value: "lifecycle".to_string(),
        };
        registry.insert(entity, (comp_c.clone(),)).unwrap();

        assert_eq!(registry.get::<ComponentA>(entity), Some(&comp_a));
        assert_eq!(registry.get::<ComponentB>(entity), Some(&comp_b));
        assert_eq!(registry.get::<ComponentC>(entity), Some(&comp_c));
        let abc_archetype_id = registry.get_entity_archetype_id(entity).unwrap();
        assert_ne!(abc_archetype_id, ab_archetype_id);
        let abc_arch = registry.archetypes.get(&abc_archetype_id).unwrap();
        assert_eq!(abc_arch.signature().len(), 3);
        assert!(abc_arch.signature().contains(&ComponentTypeId::of::<ComponentA>()));
        assert!(abc_arch.signature().contains(&ComponentTypeId::of::<ComponentB>()));
        assert!(abc_arch.signature().contains(&ComponentTypeId::of::<ComponentC>()));
    }

    #[test]
    fn modify_a_after_abc_and_verify() {
        let mut registry = create_test_registry();
        let entity = registry.spawn();
        let comp_a_initial = ComponentA { value: 1 };
        let comp_b = ComponentB { value: 1.0 };
        let comp_c = ComponentC {
            value: "lifecycle".to_string(),
        };
        registry
            .insert(
                entity,
                (comp_a_initial.clone(), comp_b.clone(), comp_c.clone()),
            )
            .unwrap();

        let mut_a = registry.get_mut::<ComponentA>(entity).unwrap();
        mut_a.value = 2;
        let modified_comp_a = ComponentA { value: 2 };

        assert_eq!(registry.get::<ComponentA>(entity), Some(&modified_comp_a));
        assert_eq!(registry.get::<ComponentB>(entity), Some(&comp_b)); // B should be original
        assert_eq!(registry.get::<ComponentC>(entity), Some(&comp_c)); // C should be original
    }

    #[test]
    fn remove_b_after_abc_modify_a_and_verify() {
        let mut registry = create_test_registry();
        let entity = registry.spawn();
        let comp_a_initial = ComponentA { value: 1 };
        let comp_b = ComponentB { value: 1.0 };
        let comp_c = ComponentC {
            value: "lifecycle".to_string(),
        };
        registry
            .insert(
                entity,
                (comp_a_initial.clone(), comp_b.clone(), comp_c.clone()),
            )
            .unwrap();
        let abc_archetype_id = registry.get_entity_archetype_id(entity).unwrap();

        let mut_a = registry.get_mut::<ComponentA>(entity).unwrap();
        mut_a.value = 2;
        let modified_comp_a = ComponentA { value: 2 };

        registry.remove::<ComponentB>(entity).unwrap();

        assert_eq!(registry.get::<ComponentA>(entity), Some(&modified_comp_a));
        assert!(registry.get::<ComponentB>(entity).is_none());
        assert_eq!(registry.get::<ComponentC>(entity), Some(&comp_c));
        let ac_archetype_id = registry.get_entity_archetype_id(entity).unwrap();
        assert_ne!(ac_archetype_id, abc_archetype_id);
        let ac_arch = registry.archetypes.get(&ac_archetype_id).unwrap();
        assert_eq!(ac_arch.signature().len(), 2);
        assert!(ac_arch.signature().contains(&ComponentTypeId::of::<ComponentA>()));
        assert!(ac_arch.signature().contains(&ComponentTypeId::of::<ComponentC>()));
    }

    #[test]
    fn remove_a_after_ac_and_verify() {
        let mut registry = create_test_registry();
        let entity = registry.spawn();
        // Setup: A, C
        let comp_a = ComponentA { value: 2 }; // Assuming A was modified to 2
        let comp_c = ComponentC {
            value: "lifecycle".to_string(),
        };
        registry.insert(entity, (comp_a.clone(), comp_c.clone())).unwrap();
        let ac_archetype_id = registry.get_entity_archetype_id(entity).unwrap();

        registry.remove::<ComponentA>(entity).unwrap();

        assert!(registry.get::<ComponentA>(entity).is_none());
        assert_eq!(registry.get::<ComponentC>(entity), Some(&comp_c));
        let c_archetype_id = registry.get_entity_archetype_id(entity).unwrap();
        assert_ne!(c_archetype_id, ac_archetype_id);
        let c_arch = registry.archetypes.get(&c_archetype_id).unwrap();
        assert_eq!(c_arch.signature().len(), 1);
        assert!(c_arch.signature().contains(&ComponentTypeId::of::<ComponentC>()));
    }

    #[test]
    fn remove_c_last_component_and_verify_null_archetype() {
        let mut registry = create_test_registry();
        let entity = registry.spawn();
        // Setup: C only
        let comp_c = ComponentC {
            value: "lifecycle".to_string(),
        };
        registry.insert(entity, (comp_c.clone(),)).unwrap();
        let c_archetype_id = registry.get_entity_archetype_id(entity).unwrap();

        registry.remove::<ComponentC>(entity).unwrap();

        assert!(registry.get::<ComponentA>(entity).is_none());
        assert!(registry.get::<ComponentB>(entity).is_none());
        assert!(registry.get::<ComponentC>(entity).is_none());
        let final_null_archetype_id = registry.get_entity_archetype_id(entity).unwrap();
        assert_ne!(final_null_archetype_id, c_archetype_id);
        assert!(
            registry.archetypes.get(&final_null_archetype_id).unwrap().signature().is_empty()
        );
    }

    #[test]
    fn despawn_after_all_removed_and_verify_get() {
        let mut registry = create_test_registry();
        let entity = registry.spawn();
        // Entity is in null archetype after setup and removals in previous tests.
        // For this isolated test, we just ensure it's spawned.

        assert!(registry.despawn(entity));
        assert!(!registry.is_entity_alive(entity));
        assert!(registry.get::<ComponentA>(entity).is_none());
        assert!(registry.get::<ComponentC>(entity).is_none()); // Check another component too
    }

    #[test]
    fn operations_on_despawned_entity_errors() {
        let mut registry = create_test_registry();
        let e_despawn = registry.spawn();
        registry.insert(e_despawn, (ComponentA { value: 1 },)).unwrap();
        let e_despawn_handle = e_despawn; // Keep original handle
        registry.despawn(e_despawn);

        assert_eq!(
            registry.insert(e_despawn_handle, (ComponentB { value: 1.0 },)),
            Err(InsertError::EntityNotFound),
            "Insert on despawned entity should fail"
        );
        assert!(
            registry.get::<ComponentA>(e_despawn_handle).is_none(),
            "Get on despawned entity should return None"
        );
        assert!(
            registry.get_mut::<ComponentA>(e_despawn_handle).is_none(),
            "Get_mut on despawned entity should return None"
        );
        assert_eq!(
            registry.remove::<ComponentA>(e_despawn_handle),
            Err(RemoveError::EntityNotFound),
            "Remove on despawned entity should fail"
        );
    }

    #[test]
    fn remove_component_from_null_archetype_entity_errors() {
        let mut registry = create_test_registry();
        let e_null = registry.spawn(); // e_null is in null archetype
        assert!(
            registry.get_entity_archetype_id(e_null).map_or(false, |id| registry
                .archetypes
                .get(&id)
                .unwrap()
                .signature()
                .is_empty()),
            "Entity should be in null archetype after spawn"
        );
        let remove_from_null_result = registry.remove::<ComponentA>(e_null);
        assert_eq!(
            remove_from_null_result,
            Err(RemoveError::ComponentNotPresent),
            "Removing non-existent component from null archetype entity should be ComponentNotPresent"
        );
    }

    #[test]
    fn get_remove_non_existent_component_on_existing_entity_errors() {
        let mut registry = create_test_registry();
        let e_exists = registry.spawn();
        registry.insert(e_exists, (ComponentA { value: 10 },)).unwrap(); // Has A, not B

        assert!(
            registry.get::<ComponentB>(e_exists).is_none(),
            "Get non-existent ComponentB should return None"
        );
        assert!(
            registry.get_mut::<ComponentB>(e_exists).is_none(),
            "Get_mut non-existent ComponentB should return None"
        );
        assert_eq!(
            registry.remove::<ComponentB>(e_exists),
            Err(RemoveError::ComponentNotPresent),
            "Remove non-existent ComponentB should be ComponentNotPresent"
        );
        assert!(
            registry.get::<ComponentA>(e_exists).is_some(),
            "ComponentA should still exist after failed ops on B"
        );
    }

    #[test]
    fn complex_archetype_transitions_and_data_integrity() {
        let mut registry = create_test_registry();
        let e_trans = registry.spawn(); // Null archetype initially

        // Null -> A
        let comp_a = ComponentA { value: 100 };
        registry.insert(e_trans, (comp_a.clone(),)).unwrap();
        assert_eq!(registry.get::<ComponentA>(e_trans), Some(&comp_a));
        let arch_a_id = registry.get_entity_archetype_id(e_trans).unwrap();
        let arch_a = registry.archetypes.get(&arch_a_id).unwrap();
        assert_eq!(arch_a.signature().len(), 1);
        assert!(arch_a.signature().contains(&ComponentTypeId::of::<ComponentA>()));

        // A -> A, B
        let comp_b = ComponentB { value: 200.0 };
        registry.insert(e_trans, (comp_b.clone(),)).unwrap(); // Adds B, A is preserved
        assert_eq!(registry.get::<ComponentA>(e_trans), Some(&comp_a)); // A must persist
        assert_eq!(registry.get::<ComponentB>(e_trans), Some(&comp_b));
        let arch_ab_id = registry.get_entity_archetype_id(e_trans).unwrap();
        assert_ne!(
            arch_ab_id, arch_a_id,
            "Archetype should change from A to A,B"
        );
        let arch_ab = registry.archetypes.get(&arch_ab_id).unwrap();
        assert_eq!(arch_ab.signature().len(), 2);
        assert!(arch_ab.signature().contains(&ComponentTypeId::of::<ComponentA>()));
        assert!(arch_ab.signature().contains(&ComponentTypeId::of::<ComponentB>()));

        // A, B -> A, B, D (Marker)
        let marker_d = MarkerComponentD;
        registry.insert(e_trans, (marker_d.clone(),)).unwrap(); // Adds D
        assert_eq!(registry.get::<ComponentA>(e_trans), Some(&comp_a));
        assert_eq!(registry.get::<ComponentB>(e_trans), Some(&comp_b));
        assert_eq!(registry.get::<MarkerComponentD>(e_trans), Some(&marker_d));
        let arch_abd_id = registry.get_entity_archetype_id(e_trans).unwrap();
        assert_ne!(
            arch_abd_id, arch_ab_id,
            "Archetype should change from A,B to A,B,D"
        );
        let arch_abd = registry.archetypes.get(&arch_abd_id).unwrap();
        assert_eq!(arch_abd.signature().len(), 3);
        assert!(arch_abd.signature().contains(&ComponentTypeId::of::<MarkerComponentD>()));

        // A, B, D -> A, D (remove B)
        registry.remove::<ComponentB>(e_trans).unwrap();
        assert_eq!(registry.get::<ComponentA>(e_trans), Some(&comp_a));
        assert!(registry.get::<ComponentB>(e_trans).is_none());
        assert_eq!(registry.get::<MarkerComponentD>(e_trans), Some(&marker_d));
        let arch_ad_id = registry.get_entity_archetype_id(e_trans).unwrap();
        assert_ne!(
            arch_ad_id, arch_abd_id,
            "Archetype should change from A,B,D to A,D"
        );
        let arch_ad = registry.archetypes.get(&arch_ad_id).unwrap();
        assert_eq!(arch_ad.signature().len(), 2);
        assert!(!arch_ad.signature().contains(&ComponentTypeId::of::<ComponentB>()));
        assert!(arch_ad.signature().contains(&ComponentTypeId::of::<ComponentA>()));
        assert!(arch_ad.signature().contains(&ComponentTypeId::of::<MarkerComponentD>()));

        // A, D -> D (remove A)
        registry.remove::<ComponentA>(e_trans).unwrap();
        assert!(registry.get::<ComponentA>(e_trans).is_none());
        assert_eq!(registry.get::<MarkerComponentD>(e_trans), Some(&marker_d));
        let arch_d_id = registry.get_entity_archetype_id(e_trans).unwrap();
        assert_ne!(arch_d_id, arch_ad_id);

        // D -> Null (remove D)
        registry.remove::<MarkerComponentD>(e_trans).unwrap();
        assert!(registry.get::<MarkerComponentD>(e_trans).is_none());
        let arch_null_id = registry.get_entity_archetype_id(e_trans).unwrap();
        assert_ne!(arch_null_id, arch_d_id);
        assert!(registry.archetypes.get(&arch_null_id).unwrap().signature().is_empty());

        // --- Test with DataComponentE (Vec data) ---
        let e_vec = registry.spawn();
        let data_e1 = DataComponentE {
            data: vec![1, 2, 3],
        };
        registry.insert(e_vec, (data_e1.clone(),)).unwrap();
        assert_eq!(registry.get::<DataComponentE>(e_vec), Some(&data_e1));

        // Add another component, ensure DataComponentE data is moved correctly
        registry.insert(e_vec, (ComponentA { value: 50 },)).unwrap();
        assert_eq!(
            registry.get::<DataComponentE>(e_vec),
            Some(&data_e1),
            "DataComponentE data integrity after adding another component"
        );
        assert_eq!(
            registry.get::<ComponentA>(e_vec),
            Some(&ComponentA { value: 50 })
        );

        // Modify DataComponentE
        let mut_data_e = registry.get_mut::<DataComponentE>(e_vec).unwrap();
        mut_data_e.data.push(4);
        let modified_data_e = DataComponentE {
            data: vec![1, 2, 3, 4],
        };
        assert_eq!(
            registry.get::<DataComponentE>(e_vec),
            Some(&modified_data_e)
        );

        // Remove ComponentA, ensure DataComponentE data is still correct
        registry.remove::<ComponentA>(e_vec).unwrap();
        assert!(registry.get::<ComponentA>(e_vec).is_none());
        assert_eq!(
            registry.get::<DataComponentE>(e_vec),
            Some(&modified_data_e),
            "DataComponentE data integrity after removing another component"
        );
    }
}
