use std::collections::{BTreeSet, HashMap, VecDeque};

use crate::archetype::{AnyStorage, Archetype, ArchetypeId};
use crate::component::ComponentTypeId;
use crate::entity::Entity;

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
            free_entities: VecDeque::new(),
            next_entity_id: 0,
            entity_generations: Vec::new(),
        }
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
    /// IMPORTANT: This function currently can only create archetypes with NO components.
    /// Attempting to create an archetype for a non-empty set of component types will panic,
    /// as the mechanism to instantiate the correctly typed `Storage<T>` objects from
    /// `ComponentTypeId`s is not yet implemented here. That logic will be part of
    /// component addition methods that have generic type information.
    ///
    /// # Arguments
    /// * `component_types_set` - A `BTreeSet` of `ComponentTypeId`s representing the desired signature.
    ///
    /// # Returns
    /// The `ArchetypeId` of the found or created archetype.
    ///
    /// # Panics
    /// Panics if `component_types_set` is not empty, as storage creation is not yet supported here.
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

            // This is the critical point: creating the actual Storage<T> instances.
            // Without a factory mechanism or generic context here, we cannot know what `T` to use.
            let storages: Vec<Box<dyn AnyStorage>>;
            if component_types_vec.is_empty() {
                storages = Vec::new();
            } else {
                // This function, in its current isolated form, cannot create these.
                // This responsibility will be handled by higher-level functions
                // (e.g., when adding components with known types).
                // For now, to make progress and allow testing of ID management for empty archetypes:
                panic!(
                    "find_or_create_archetype cannot yet create storages for non-empty signatures ({:?}). This requires a component factory mechanism.",
                    component_types_vec
                );
            }

            let archetype = Archetype::new(new_archetype_id, component_types_vec, storages);
            self.archetypes.insert(new_archetype_id, archetype);
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
        expected = "find_or_create_archetype cannot yet create storages for non-empty signatures"
    )]
    fn find_or_create_archetype_non_empty_signature_panics() {
        let mut registry = Registry::new();
        let mut sig1 = BTreeSet::new();
        sig1.insert(ComponentTypeId::of::<Position>());

        // This call should panic due to the current implementation
        registry.find_or_create_archetype(sig1);
    }
}
