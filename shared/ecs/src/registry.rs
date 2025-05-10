use std::collections::{BTreeSet, HashMap, VecDeque};

use crate::archetype::{AnyStorage, Archetype, ArchetypeId, Storage};
use crate::component::{Component, ComponentBundle, ComponentTypeId};
use crate::entity::Entity;
use std::any::Any;

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
                self.entity_generations.resize(entity_id_val + 1, 0); // Default to 0, then increment.
            }
            self.entity_generations[entity_id_val] += 1; // Increment generation on reuse
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
        self.entity_generations[entity.id()] += 1;
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
        let mut new_components_data = bundle.into_components_data();

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
    /// * `entity` - The `Entity` whose component is to be retrieved.
    ///
    /// # Returns
    /// * `Some(&T)` if the entity is alive, belongs to an archetype that has component `T`,
    ///   and the component data is found.
    /// * `None` otherwise (e.g., entity not alive, entity doesn't have component `T`).
    ///
    /// # Panics
    /// Panics internally if the ECS is in an inconsistent state (e.g., entity location points
    /// to a non-existent archetype, or storage downcast fails unexpectedly).
    pub fn get_component<T: Component>(&self, entity: Entity) -> Option<&T> {
        if !self.is_entity_alive(entity) {
            return None;
        }

        // Get the location, ? will return None if entity is not in entity_locations
        let (archetype_id_ref, row_index_ref) = self.entity_locations.get(&entity)?;
        // Dereference to get actual values, as ArchetypeId and usize are Copy.
        let actual_archetype_id = *archetype_id_ref;
        let actual_row_index = *row_index_ref;

        let archetype = self.archetypes.get(&actual_archetype_id)?;

        // Check if the archetype even has this component type.
        if !archetype.has_component_type(&ComponentTypeId::of::<T>()) {
            return None;
        }

        // Safety: We've confirmed the entity is alive, its location is valid,
        // and the archetype is expected to contain this component type.
        unsafe {
            archetype
                .get_storage::<T>(ComponentTypeId::of::<T>())
                .and_then(|storage| storage.get(actual_row_index))
        }
    }

    /// Retrieves a mutable reference to a component of type `T` for a given entity.
    ///
    /// # Arguments
    /// * `entity` - The `Entity` whose component is to be retrieved.
    ///
    /// # Returns
    /// * `Some(&mut T)` if the entity is alive, belongs to an archetype that has component `T`,
    ///   and the component data is found.
    /// * `None` otherwise (e.g., entity not alive, entity doesn't have component `T`).
    ///
    /// # Panics
    /// Panics internally if the ECS is in an inconsistent state.
    pub fn get_component_mut<T: Component>(&mut self, entity: Entity) -> Option<&mut T> {
        if !self.is_entity_alive(entity) {
            return None;
        }

        let (archetype_id, row_index) = self.entity_locations.get(&entity)?;

        // Need to get archetype_id and row_index before the mutable borrow of self.archetypes
        let archetype_id_cloned = *archetype_id;
        let row_index_cloned = *row_index;

        let archetype = self.archetypes.get_mut(&archetype_id_cloned)?;

        if !archetype.has_component_type(&ComponentTypeId::of::<T>()) {
            return None;
        }

        // Safety: Similar to get_component.
        unsafe {
            archetype
                .get_storage_mut::<T>(ComponentTypeId::of::<T>())
                .and_then(|storage| storage.get_mut(row_index_cloned))
        }
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
    pub fn remove_component<T: Component>(
        &mut self,
        entity: Entity,
    ) -> Result<T, RemoveError> {
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
            Ok(component_instance) => Ok(*component_instance),
            Err(_) => Err(RemoveError::DowncastFailed), // Should not happen if types are consistent
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::archetype::*; // Ensure ArchetypeId is in scope for tests
    use crate::component::Component; // Make sure Component is in scope for tests
    use void_architect_ecs_macros::Component;

    // Test components (already defined in archetype.rs tests, but good to have here too for clarity if needed)
    #[derive(Component, Debug, Clone, PartialEq)]
    struct Position {
        x: f32,
        y: f32,
    }

    #[derive(Component, Debug, Clone, PartialEq)]
    struct Velocity {
        dx: f32,
        dy: f32,
    }

    // A dummy component that might be needed if we were to try to create non-empty storages in tests
    // without a full factory. Not used with the current panic approach.
    // #[derive(Debug, Clone, PartialEq)]
    // struct DummyComponentForTest;
    // impl Component for DummyComponentForTest {}

    #[test]
    fn registry_new() {
        let registry = Registry::new();
        assert!(registry.archetypes.is_empty());
        assert!(registry.signature_to_archetype_id.is_empty());
        assert_eq!(registry.next_archetype_id, 0);
        assert!(registry.entity_locations.is_empty());
        assert!(registry.free_entities.is_empty());
        assert_eq!(registry.next_entity_id, 0);
        assert!(registry.entity_generations.is_empty());
    }

    #[test]
    fn registry_spawn_entity() {
        let mut registry = Registry::new();
        let entity1 = registry.spawn();
        let entity2 = registry.spawn();

        assert_ne!(entity1, entity2);
        assert_eq!(entity1.id(), 0);
        assert_eq!(entity1.generation(), 0);
        assert_eq!(entity2.id(), 1);
        assert_eq!(entity2.generation(), 0);

        // Check internal state for entity allocation
        assert_eq!(registry.next_entity_id, 2);
        assert_eq!(registry.entity_generations.len(), 2);
        assert_eq!(registry.entity_generations[0], 0);
        assert_eq!(registry.entity_generations[1], 0);
        assert!(registry.free_entities.is_empty());

        // Check entity location (should be in null archetype)
        let null_archetype_signature = BTreeSet::new();
        let null_archetype_id =
            registry.signature_to_archetype_id.get(&null_archetype_signature).unwrap();

        assert_eq!(
            registry.entity_locations.get(&entity1),
            Some(&(*null_archetype_id, 0))
        ); // entity1 is row 0 in null archetype
        assert_eq!(
            registry.entity_locations.get(&entity2),
            Some(&(*null_archetype_id, 1))
        ); // entity2 is row 1 in null archetype

        let archetype = registry.archetypes.get(null_archetype_id).unwrap();
        assert_eq!(archetype.entities_count(), 2);
        assert!(archetype.component_types().is_empty());
    }

    #[test]
    fn registry_despawn_and_reuse_entity() {
        let mut registry = Registry::new();
        let entity1 = registry.spawn(); // id 0, gen 0
        let entity2 = registry.spawn(); // id 1, gen 0

        // Entity 1 should be in null archetype at row 0
        // Entity 2 should be in null archetype at row 1
        let null_archetype_id =
            *registry.signature_to_archetype_id.get(&BTreeSet::new()).unwrap();

        let initial_null_archetype_entity_count =
            registry.archetypes.get(&null_archetype_id).unwrap().entities_count();
        assert_eq!(initial_null_archetype_entity_count, 2);
        assert_eq!(
            registry.archetypes.get(&null_archetype_id).unwrap().get_entity_by_row(0),
            Some(entity1)
        );
        assert_eq!(
            registry.archetypes.get(&null_archetype_id).unwrap().get_entity_by_row(1),
            Some(entity2)
        );

        assert!(registry.despawn(entity1)); // Despawn id 0, gen 0. After despawn, generation becomes 1.
        assert!(!registry.is_entity_alive(entity1)); // Check original handle
        assert_eq!(registry.entity_generations[0], 1); // Generation incremented
        assert_eq!(registry.free_entities.len(), 1);
        assert_eq!(registry.free_entities[0], 0); // ID 0 is now free
        assert!(!registry.entity_locations.contains_key(&entity1)); // Should be removed from locations

        // Check null archetype after despawning entity1
        // Entity2 (original id 1) should have been swapped to row 0 in the null archetype.
        let null_archetype = registry.archetypes.get(&null_archetype_id).unwrap();
        assert_eq!(null_archetype.entities_count(), 1);
        assert_eq!(null_archetype.get_entity_by_row(0), Some(entity2)); // entity2 now at row 0
        assert_eq!(
            registry.entity_locations.get(&entity2),
            Some(&(null_archetype_id, 0))
        );

        let entity3 = registry.spawn(); // Should reuse ID 0, gen 2
        assert_eq!(entity3.id(), 0); // Reused ID
        assert_eq!(entity3.generation(), 2); // Incremented generation, which should be 2 now.
        assert!(registry.is_entity_alive(entity3));
        assert!(registry.free_entities.is_empty());
        assert_eq!(registry.entity_generations[0], 2); // Stays gen 2

        // Entity3 (id 0, gen 2) should now be in the null archetype, likely at row 1 (after entity2)
        let null_archetype_after_entity3_spawn =
            registry.archetypes.get(&null_archetype_id).unwrap();
        assert_eq!(null_archetype_after_entity3_spawn.entities_count(), 2);
        assert_eq!(
            registry.entity_locations.get(&entity3),
            Some(&(null_archetype_id, 1))
        );
        assert_eq!(
            null_archetype_after_entity3_spawn.get_entity_by_row(1),
            Some(entity3)
        );

        let entity4 = registry.spawn(); // Should use new ID 2, as entity2 (id 1) is still alive
        assert_eq!(entity4.id(), 2);
        assert_eq!(entity4.generation(), 0);
        assert!(registry.is_entity_alive(entity4));

        assert!(registry.despawn(entity2)); // Despawn id 1, gen 0. Gen becomes 1.
        assert!(!registry.is_entity_alive(entity2));
        assert_eq!(registry.entity_generations[1], 1);

        assert!(registry.despawn(entity3)); // Despawn id 0, gen 2. Gen becomes 3.
        assert!(!registry.is_entity_alive(entity3));
        assert_eq!(registry.entity_generations[0], 3);

        // State of the null archetype after despawning entities:
        //
        // 1. Despawning entity2 (original row 0):
        //    - Entity4 (row 2, added last before despawns) is swapped to row 0.
        //    - Null archetype state:
        //      [entity4 (id2), entity3 (id0, gen1)]
        //    - entity_locations:
        //      {entity4: (null, 0), entity3: (null, 1)}
        //
        // 2. Despawning entity3 (original row 1):
        //    - No swap occurs as it was the last entity.
        //    - Null archetype state:
        //      [entity4 (id2)]
        //    - entity_locations:
        //      {entity4: (null, 0)}

        let null_archetype_final = registry.archetypes.get(&null_archetype_id).unwrap();
        // Spawn a new entity, reusing ID 1 (from despawned entity2) with generation incremented to 2.
        let entity5 = registry.spawn();
        assert_eq!(entity5.id(), 1);
        assert_eq!(entity5.generation(), 2);
        assert!(registry.is_entity_alive(entity5));

        let entity6 = registry.spawn(); // Spawns entity 0, gen 4 (reused from despawned entity3)
        assert_eq!(entity6.id(), 0);
        assert_eq!(entity6.generation(), 4);
        assert!(registry.is_entity_alive(entity6));
    }

    #[test]
    fn registry_despawn_non_existent_entity() {
        let mut registry = Registry::new();
        let entity_never_spawned = Entity::new(0, 0);
        assert!(!registry.despawn(entity_never_spawned)); // Not created yet

        let entity1 = registry.spawn(); // Spawns entity 0, gen 0

        assert!(registry.despawn(entity1)); // Despawn 0, gen 0. Now gen is 1 for ID 0.
        assert!(!registry.is_entity_alive(entity1)); // Original handle no longer alive

        // Attempt to despawn with the old handle (correct ID, outdated generation)
        assert!(!registry.despawn(entity1)); // Fails because generation doesn't match

        let entity_bad_id = Entity::new(100, 0); // ID that was never used
        assert!(!registry.despawn(entity_bad_id));
    }

    #[test]
    fn registry_is_entity_alive_after_spawn_despawn() {
        let mut registry = Registry::new();
        let entity1 = registry.spawn();
        assert!(registry.is_entity_alive(entity1));

        registry.despawn(entity1);
        assert!(!registry.is_entity_alive(entity1)); // Original handle is no longer alive

        let entity_invalid_id = Entity::new(100, 0); // ID not yet allocated
        assert!(!registry.is_entity_alive(entity_invalid_id));

        let entity_reused = registry.spawn(); // Reuses entity1's ID (0), but with new generation (1)
        assert!(registry.is_entity_alive(entity_reused));
        assert_eq!(entity_reused.id(), entity1.id());
        assert_ne!(entity_reused.generation(), entity1.generation());
        assert!(!registry.is_entity_alive(entity1)); // Original handle (gen 0) still not alive
    }

    #[test]
    fn find_or_create_archetype_empty_signature() {
        let mut registry = Registry::new();
        let empty_sig = BTreeSet::new();

        let arch_id1 = registry.find_or_create_archetype(empty_sig.clone());
        assert_eq!(arch_id1, ArchetypeId::new(0));
        assert!(registry.archetypes.contains_key(&arch_id1));
        assert_eq!(
            registry.archetypes.get(&arch_id1).unwrap().component_types().len(),
            0
        );
        assert_eq!(
            registry.signature_to_archetype_id.get(&empty_sig),
            Some(&arch_id1)
        );

        let found_arch_id1 = registry.find_or_create_archetype(empty_sig.clone());
        assert_eq!(found_arch_id1, arch_id1);
        assert_eq!(registry.next_archetype_id, 1); // Should not have incremented further for empty
    }

    #[test]
    #[should_panic(
        expected = "Attempted to create archetype with unregistered component type: ComponentTypeId"
    )]
    fn find_or_create_archetype_with_unregistered_component_panics() {
        let mut registry = Registry::new();
        // Position is NOT registered
        let mut sig_with_unregistered = BTreeSet::new();
        sig_with_unregistered.insert(ComponentTypeId::of::<Position>());

        // This call should panic because Position's factory isn't registered.
        registry.find_or_create_archetype(sig_with_unregistered);
    }

    #[test]
    fn find_or_create_archetype_with_registered_component_succeeds() {
        let mut registry = Registry::new();
        registry.register_component::<Position>(); // Register Position

        let mut sig_with_registered = BTreeSet::new();
        sig_with_registered.insert(ComponentTypeId::of::<Position>());

        let arch_id = registry.find_or_create_archetype(sig_with_registered.clone());
        assert!(registry.archetypes.contains_key(&arch_id));
        let archetype = registry.archetypes.get(&arch_id).unwrap();
        assert_eq!(archetype.component_types().len(), 1);
        assert_eq!(
            archetype.component_types()[0],
            ComponentTypeId::of::<Position>()
        );

        // Calling again should return the same ID
        let found_arch_id = registry.find_or_create_archetype(sig_with_registered);
        assert_eq!(found_arch_id, arch_id);
    }

    #[test]
    fn registry_insert_for_dead_entity_errors() {
        let mut registry = Registry::new();
        registry.register_component::<Position>();
        let entity = registry.spawn();
        registry.despawn(entity);

        let result = registry.insert(entity, (Position { x: 0.0, y: 0.0 },));
        assert_eq!(result, Err(InsertError::EntityNotFound));
    }

    #[test]
    fn registry_insert_unregistered_component_errors() {
        let mut registry = Registry::new();
        // Position is NOT registered
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
        let mut registry = Registry::new();
        registry.register_component::<Position>();
        registry.register_component::<Velocity>();

        let e1 = registry.spawn(); // Null
        let e2 = registry.spawn(); // Null

        // e1 gets Position
        registry.insert(e1, (Position { x: 1.0, y: 1.0 },)).unwrap();
        let e1_arch_id_pos = registry.get_entity_archetype_id(e1).unwrap();
        let e1_arch_pos = registry.archetypes.get(&e1_arch_id_pos).unwrap();
        assert_eq!(e1_arch_pos.signature().len(), 1);
        assert!(e1_arch_pos.signature().contains(&ComponentTypeId::of::<Position>()));

        // e2 gets Velocity
        registry.insert(e2, (Velocity { dx: 2.0, dy: 2.0 },)).unwrap();
        let e2_arch_id_vel = registry.get_entity_archetype_id(e2).unwrap();
        let e2_arch_vel = registry.archetypes.get(&e2_arch_id_vel).unwrap();
        assert_eq!(e2_arch_vel.signature().len(), 1);
        assert!(e2_arch_vel.signature().contains(&ComponentTypeId::of::<Velocity>()));
        assert_ne!(
            e1_arch_id_pos, e2_arch_id_vel,
            "e1 and e2 should be in different archetypes."
        );

        // e1 gets Velocity (now has Position, Velocity)
        registry.insert(e1, (Velocity { dx: 1.1, dy: 1.1 },)).unwrap();
        let e1_arch_id_pos_vel = registry.get_entity_archetype_id(e1).unwrap();
        let e1_arch_pos_vel = registry.archetypes.get(&e1_arch_id_pos_vel).unwrap();
        assert_eq!(e1_arch_pos_vel.signature().len(), 2);
        assert!(e1_arch_pos_vel.signature().contains(&ComponentTypeId::of::<Position>()));
        assert!(e1_arch_pos_vel.signature().contains(&ComponentTypeId::of::<Velocity>()));
        assert_ne!(e1_arch_id_pos_vel, e1_arch_id_pos);
        assert_ne!(e1_arch_id_pos_vel, e2_arch_id_vel);

        // Check original Position-only archetype for e1 is now empty
        let e1_original_pos_archetype = registry.archetypes.get(&e1_arch_id_pos).unwrap();
        assert_eq!(e1_original_pos_archetype.entities_count(), 0);

        // e2 gets Position (now has Velocity, Position) - should end up in same archetype as e1
        registry.insert(e2, (Position { x: 2.2, y: 2.2 },)).unwrap();
        let e2_arch_id_vel_pos = registry.get_entity_archetype_id(e2).unwrap();
        assert_eq!(
            e2_arch_id_vel_pos, e1_arch_id_pos_vel,
            "e1 and e2 should now be in the same (Pos,Vel) archetype."
        );

        let final_pos_vel_archetype = registry.archetypes.get(&e1_arch_id_pos_vel).unwrap();
        assert_eq!(final_pos_vel_archetype.entities_count(), 2);

        // Check original Velocity-only archetype for e2 is now empty
        let e2_original_vel_archetype = registry.archetypes.get(&e2_arch_id_vel).unwrap();
        assert_eq!(e2_original_vel_archetype.entities_count(), 0);

        // TODO: Verify component data with get<C>
        assert_eq!(
            *registry.get_component::<Position>(e1).unwrap(),
            Position { x: 1.0, y: 1.0 }
        );
        assert_eq!(
            *registry.get_component::<Velocity>(e1).unwrap(),
            Velocity { dx: 1.1, dy: 1.1 }
        );
        assert_eq!(
            *registry.get_component::<Position>(e2).unwrap(),
            Position { x: 2.2, y: 2.2 }
        );
        assert_eq!(
            *registry.get_component::<Velocity>(e2).unwrap(),
            Velocity { dx: 2.0, dy: 2.0 }
        );
    }

    #[test]
    fn registry_get_component_and_get_component_mut() {
        let mut registry = Registry::new();
        registry.register_component::<Position>();
        registry.register_component::<Velocity>();

        let entity = registry.spawn();

        // 1. Get non-existent component
        assert!(registry.get_component::<Position>(entity).is_none());
        assert!(registry.get_component_mut::<Position>(entity).is_none());

        // Insert Position
        let pos1 = Position { x: 1.0, y: 2.0 };
        registry.insert(entity, (pos1.clone(),)).unwrap();

        // 2. Get existing component
        let retrieved_pos = registry.get_component::<Position>(entity).unwrap();
        assert_eq!(*retrieved_pos, pos1);

        // 3. Get_mut and modify component
        let retrieved_pos_mut = registry.get_component_mut::<Position>(entity).unwrap();
        assert_eq!(*retrieved_pos_mut, pos1);
        retrieved_pos_mut.x = 10.0;
        retrieved_pos_mut.y = 20.0;
        let modified_pos = Position { x: 10.0, y: 20.0 };

        // Verify modification
        let retrieved_pos_after_mut = registry.get_component::<Position>(entity).unwrap();
        assert_eq!(*retrieved_pos_after_mut, modified_pos);

        // 4. Get component that exists, but not on this entity (after adding another)
        let entity2 = registry.spawn();
        registry.insert(entity2, (Velocity { dx: 1.0, dy: 1.0 },)).unwrap();
        assert!(
            registry.get_component::<Velocity>(entity).is_none(),
            "Entity 1 should not have Velocity"
        );
        assert!(
            registry.get_component::<Position>(entity2).is_none(),
            "Entity 2 should not have Position"
        );
        assert!(
            registry.get_component::<Velocity>(entity2).is_some(),
            "Entity 2 should have Velocity"
        );

        // 5. Get component from a dead entity
        registry.despawn(entity);
        assert!(registry.get_component::<Position>(entity).is_none());
        assert!(registry.get_component_mut::<Position>(entity).is_none());
    }

    // Update existing tests to use get_component for verification

    #[test]
    fn registry_insert_components_to_null_archetype_entity_with_get_verification() {
        let mut registry = Registry::new();
        registry.register_component::<Position>();
        registry.register_component::<Velocity>();

        let entity = registry.spawn();
        let initial_null_archetype_id = registry.get_entity_archetype_id(entity).unwrap();

        let position_comp = Position { x: 1.0, y: 2.0 };
        registry.insert(entity, (position_comp.clone(),)).unwrap();

        assert_eq!(
            *registry.get_component::<Position>(entity).unwrap(),
            position_comp
        );
        let pos_archetype_id = registry.get_entity_archetype_id(entity).unwrap();
        assert_ne!(pos_archetype_id, initial_null_archetype_id);
        // ... (rest of the assertions about archetype structure)
    }

    #[test]
    fn registry_insert_components_in_place_update_with_get_verification() {
        let mut registry = Registry::new();
        registry.register_component::<Position>();
        let entity = registry.spawn();

        let initial_pos = Position { x: 1.0, y: 2.0 };
        registry.insert(entity, (initial_pos.clone(),)).unwrap();
        assert_eq!(
            *registry.get_component::<Position>(entity).unwrap(),
            initial_pos
        );

        let updated_pos = Position { x: 10.0, y: 20.0 };
        registry.insert(entity, (updated_pos.clone(),)).unwrap();
        assert_eq!(
            *registry.get_component::<Position>(entity).unwrap(),
            updated_pos
        );
    }

    #[test]
    fn registry_insert_mixed_new_and_existing_components_with_get_verification() {
        let mut registry = Registry::new();
        registry.register_component::<Position>();
        registry.register_component::<Velocity>();
        let entity = registry.spawn();

        let pos_comp = Position { x: 1.0, y: 2.0 };
        registry.insert(entity, (pos_comp.clone(),)).unwrap();
        assert_eq!(
            *registry.get_component::<Position>(entity).unwrap(),
            pos_comp
        );
        assert!(registry.get_component::<Velocity>(entity).is_none());

        let updated_pos = Position { x: 5.0, y: 5.0 };
        let vel_comp = Velocity { dx: 10.0, dy: 10.0 };
        registry.insert(entity, (updated_pos.clone(), vel_comp.clone())).unwrap();

        assert_eq!(
            *registry.get_component::<Position>(entity).unwrap(),
            updated_pos
        );
        assert_eq!(
            *registry.get_component::<Velocity>(entity).unwrap(),
            vel_comp
        );
    }

    #[test]
    fn registry_remove_component_success_and_archetype_change() {
        let mut registry = Registry::new();
        registry.register_component::<Position>();
        registry.register_component::<Velocity>();
        let entity = registry.spawn();

        let pos_comp = Position { x: 1.0, y: 2.0 };
        let vel_comp = Velocity { dx: 3.0, dy: 4.0 };
        registry.insert(entity, (pos_comp.clone(), vel_comp.clone())).unwrap();

        let initial_archetype_id = registry.get_entity_archetype_id(entity).unwrap();
        let initial_archetype = registry.archetypes.get(&initial_archetype_id).unwrap();
        assert_eq!(initial_archetype.signature().len(), 2);

        // Remove Position
        let removed_pos = registry.remove_component::<Position>(entity).unwrap();
        assert_eq!(removed_pos, pos_comp);

        // Check entity state after removing Position
        assert!(registry.get_component::<Position>(entity).is_none());
        let current_vel = registry.get_component::<Velocity>(entity).unwrap();
        assert_eq!(*current_vel, vel_comp); // Velocity should still be there and correct

        let archetype_id_after_pos_remove = registry.get_entity_archetype_id(entity).unwrap();
        assert_ne!(archetype_id_after_pos_remove, initial_archetype_id);
        let archetype_after_pos_remove =
            registry.archetypes.get(&archetype_id_after_pos_remove).unwrap();
        assert_eq!(archetype_after_pos_remove.signature().len(), 1);
        assert!(
            archetype_after_pos_remove
                .signature()
                .contains(&ComponentTypeId::of::<Velocity>())
        );
        assert_eq!(archetype_after_pos_remove.entities_count(), 1);

        // Check old archetype is empty
        assert_eq!(
            registry.archetypes.get(&initial_archetype_id).unwrap().entities_count(),
            0
        );

        // Remove Velocity (the last component)
        let removed_vel = registry.remove_component::<Velocity>(entity).unwrap();
        assert_eq!(removed_vel, vel_comp);

        // Check entity state after removing Velocity
        assert!(registry.get_component::<Position>(entity).is_none());
        assert!(registry.get_component::<Velocity>(entity).is_none());

        let final_archetype_id = registry.get_entity_archetype_id(entity).unwrap();
        assert_ne!(final_archetype_id, archetype_id_after_pos_remove);
        let final_archetype = registry.archetypes.get(&final_archetype_id).unwrap();
        assert!(
            final_archetype.signature().is_empty(),
            "Entity should be in null archetype"
        );
        assert_eq!(final_archetype.entities_count(), 1);

        // Check previous archetype (Velocity only) is empty
        assert_eq!(
            registry.archetypes.get(&archetype_id_after_pos_remove).unwrap().entities_count(),
            0
        );
    }

    #[test]
    fn registry_remove_component_not_present_on_entity() {
        let mut registry = Registry::new();
        registry.register_component::<Position>();
        registry.register_component::<Velocity>(); // Velocity is registered but not added to entity
        let entity = registry.spawn();

        let pos_comp = Position { x: 1.0, y: 2.0 };
        registry.insert(entity, (pos_comp.clone(),)).unwrap(); // Entity only has Position

        let result = registry.remove_component::<Velocity>(entity);
        assert_eq!(result, Err(RemoveError::ComponentNotPresent));

        // Ensure Position is still there
        assert_eq!(
            *registry.get_component::<Position>(entity).unwrap(),
            pos_comp
        );
        let current_archetype_id = registry.get_entity_archetype_id(entity).unwrap();
        let current_archetype = registry.archetypes.get(&current_archetype_id).unwrap();
        assert_eq!(current_archetype.signature().len(), 1);
        assert!(current_archetype.signature().contains(&ComponentTypeId::of::<Position>()));
    }

    #[test]
    fn registry_remove_component_from_dead_entity_errors() {
        let mut registry = Registry::new();
        registry.register_component::<Position>();
        let entity = registry.spawn();
        registry.insert(entity, (Position { x: 1.0, y: 2.0 },)).unwrap();

        let valid_entity_handle = entity; // Keep a handle before despawn
        registry.despawn(entity);

        let result = registry.remove_component::<Position>(valid_entity_handle);
        assert_eq!(result, Err(RemoveError::EntityNotFound));
    }

    #[test]
    fn registry_remove_component_unregistered_type_is_not_possible_at_compile_time() {
        // This test is more of a conceptual check.
        // `remove_component<T: Component>` requires T to be a component.
        // If T is not registered via `register_component`, `find_or_create_archetype`
        // would panic if it were ever asked to create an archetype with it.
        // However, `remove_component` itself doesn't directly check registration
        // of T, as it relies on the component already existing on the entity (which implies
        // it was registered at insertion time).
        // The primary error path is `ComponentNotPresent` if the component isn't on the entity,
        // regardless of T's registration status for the remove operation itself.
        // If one tried to remove a component type that was *never* registered system-wide,
        // it couldn't have been inserted in the first place.
        // This test primarily ensures the code compiles and doesn't have an obvious logic flaw
        // related to unregistered types during removal (which it shouldn't).
        #[derive(PartialEq, Debug)]
        struct UnregisteredComponent {}
        impl Component for UnregisteredComponent {}

        let mut registry = Registry::new();
        let entity = registry.spawn();
        // Cannot insert UnregisteredComponent if not registered, so cannot test removing it
        // if it was somehow there.
        // The relevant error is ComponentNotPresent.
        let result = registry.remove_component::<UnregisteredComponent>(entity);
        assert_eq!(result, Err(RemoveError::ComponentNotPresent));
    }
}
