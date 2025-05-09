use std::any::Any;
use std::collections::HashMap;

use crate::component::{Component, ComponentTypeId};
use crate::entity::Entity;

// --- Storage Logic ---

/// Trait for type-erased component storage.
/// Allows `Archetype` to hold collections of different `Storage<T>` types.
pub trait AnyStorage: Any + Send + Sync {
    /// Adds a component for a new entity.
    /// The caller must ensure the `component_data` is of the correct type for this storage.
    fn push_new(&mut self, component_data: Box<dyn Any>);

    /// Removes a component at a given index using swap_remove for efficiency.
    /// Returns the removed component data if successful.
    fn swap_remove(&mut self, index: usize) -> Option<Box<dyn Any>>;

    /// Returns the number of components stored (i.e., number of entities in this column).
    fn len(&self) -> usize;

    /// Returns true if the storage is empty.
    fn is_empty(&self) -> bool;

    /// Clears all components from the storage.
    fn clear(&mut self);

    /// Provides a way to downcast to `&dyn Any` for runtime type inspection if needed.
    fn as_any(&self) -> &dyn Any;

    /// Provides a way to downcast mutably to `&mut dyn Any` for runtime type inspection if needed.
    fn as_any_mut(&mut self) -> &mut dyn Any;

    /// Returns the `ComponentTypeId` of the components stored.
    /// This is crucial for the Registry to ensure type safety when creating and populating archetypes.
    fn component_type_id(&self) -> ComponentTypeId;
}

/// Concrete, typed storage for components of type `T`.
/// Implements `AnyStorage` for type erasure.
pub struct Storage<T: Component> {
    components: Vec<T>,
    component_type_id: ComponentTypeId, // Store for quick access via AnyStorage
}

impl<T: Component> Storage<T> {
    /// Creates a new, empty `Storage` for components of type `T`.
    pub fn new() -> Self {
        Self {
            components: Vec::new(),
            component_type_id: ComponentTypeId::of::<T>(),
        }
    }

    /// Gets a reference to a component at a specific index.
    pub fn get(&self, index: usize) -> Option<&T> {
        self.components.get(index)
    }

    /// Gets a mutable reference to a component at a specific index.
    pub fn get_mut(&mut self, index: usize) -> Option<&mut T> {
        self.components.get_mut(index)
    }

    /// Returns a slice containing all components.
    pub fn as_slice(&self) -> &[T] {
        &self.components
    }

    /// Returns a mutable slice containing all components.
    pub fn as_mut_slice(&mut self) -> &mut [T] {
        &mut self.components
    }
}

impl<T: Component> Default for Storage<T> {
    fn default() -> Self {
        Self::new()
    }
}

impl<T: Component> AnyStorage for Storage<T> {
    fn push_new(&mut self, component_data: Box<dyn Any>) {
        match component_data.downcast::<T>() {
            Ok(component) => self.components.push(*component),
            Err(_) => panic!(
                "Mismatched component type pushed to Storage. Expected type: {:?}, found different.",
                self.component_type_id // Using stored TypeId for better error message
            ),
        }
    }

    fn swap_remove(&mut self, index: usize) -> Option<Box<dyn Any>> {
        if index < self.components.len() {
            Some(Box::new(self.components.swap_remove(index)))
        } else {
            None
        }
    }

    fn len(&self) -> usize {
        self.components.len()
    }

    fn is_empty(&self) -> bool {
        self.components.is_empty()
    }

    fn clear(&mut self) {
        self.components.clear();
    }

    fn as_any(&self) -> &dyn Any {
        self
    }

    fn as_any_mut(&mut self) -> &mut dyn Any {
        self
    }

    fn component_type_id(&self) -> ComponentTypeId {
        self.component_type_id
    }
}

// --- Archetype Logic ---

/// A stable, unique identifier for each archetype.
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct ArchetypeId(usize);

impl ArchetypeId {
    /// Creates a new `ArchetypeId`. This is intended for internal use by the `Registry`.
    pub(crate) fn new(id: usize) -> Self {
        Self(id)
    }

    /// Returns the underlying `usize` value of the ID.
    pub fn value(self) -> usize {
        self.0
    }
}

/// Represents a unique combination of component types.
/// Stores component data in type-erased columns for entities sharing this signature.
pub struct Archetype {
    id: ArchetypeId,
    /// Sorted list of component types defining this archetype.
    /// Used for identification and to map `ComponentTypeId` to storage column index.
    component_types: Vec<ComponentTypeId>,
    /// Stores columns of component data. Each `Box<dyn AnyStorage>` is a `Storage<T>`.
    /// The order of storages matches the order in `component_types`.
    component_storage: Vec<Box<dyn AnyStorage>>,
    /// Maps an `Entity` to its row index within the component_storage vectors.
    entity_to_row_map: HashMap<Entity, usize>,
    /// Stores the entities present in this archetype, in the same order as their components.
    /// `entities[i]` corresponds to `component_storage[j].get(i)`.
    entities: Vec<Entity>,
}

impl Archetype {
    /// Creates a new `Archetype`.
    /// This is typically called by the `Registry` when a new combination of components is encountered.
    ///
    /// # Arguments
    /// * `id` - The unique ID for this archetype.
    /// * `component_types` - A vector of `ComponentTypeId`s that define this archetype. The method will sort this internally.
    /// * `storages` - A vector of `Box<dyn AnyStorage>`, one for each component type. The method will sort this internally based on `component_types`.
    ///
    /// # Panics
    /// Panics if the length of `component_types` does not match the length of `storages`.
    /// Panics if, after sorting, any `storages[i].component_type_id()` does not match `component_types[i]`. This indicates an internal logic error or inconsistent input.
    pub(crate) fn new(
        id: ArchetypeId,
        component_types: Vec<ComponentTypeId>,
        storages: Vec<Box<dyn AnyStorage>>,
    ) -> Self {
        assert_eq!(
            component_types.len(),
            storages.len(),
            "Mismatch between component types and storages count for Archetype {:?}.",
            id
        );

        // Combine component_types and storages to sort them together
        let mut combined: Vec<_> =
            component_types.into_iter().zip(storages.into_iter()).collect();

        // Sort by ComponentTypeId. This ensures that component_types will be sorted,
        // and component_storage will be in the corresponding order.
        combined.sort_unstable_by_key(|(ct_id, _)| *ct_id);

        // Separate them back
        let (sorted_component_types, sorted_storages): (Vec<_>, Vec<_>) =
            combined.into_iter().unzip();

        // Additional check: ensure sorted storages match sorted component_types
        // This check is important to verify the integrity of the input after sorting.
        for (i, ct_id) in sorted_component_types.iter().enumerate() {
            assert_eq!(
                sorted_storages[i].component_type_id(),
                *ct_id,
                "Internal error: Storage at index {} in Archetype {:?} does not match ComponentTypeId {:?} after sorting. Found {:?}.",
                i,
                id,
                ct_id,
                sorted_storages[i].component_type_id()
            );
        }

        Self {
            id,
            component_types: sorted_component_types,
            component_storage: sorted_storages,
            entity_to_row_map: HashMap::new(),
            entities: Vec::new(),
        }
    }

    /// Returns the unique ID of this archetype.
    pub fn id(&self) -> ArchetypeId {
        self.id
    }

    /// Returns a slice of the `ComponentTypeId`s that define this archetype's signature.
    /// The slice is sorted.
    pub fn component_types(&self) -> &[ComponentTypeId] {
        &self.component_types
    }

    /// Checks if this archetype contains a specific component type.
    pub fn has_component_type(&self, component_type_id: &ComponentTypeId) -> bool {
        // Assumes component_types is sorted, allowing for binary search.
        self.component_types.binary_search(component_type_id).is_ok()
    }

    /// Returns the number of entities currently stored in this archetype.
    pub fn entities_count(&self) -> usize {
        self.entities.len()
    }

    /// Returns a slice of all entities in this archetype.
    pub fn entities(&self) -> &[Entity] {
        &self.entities
    }

    /// Gets the row index for a given entity within this archetype.
    pub(crate) fn get_entity_row(&self, entity: Entity) -> Option<usize> {
        self.entity_to_row_map.get(&entity).copied()
    }

    pub(crate) fn get_entity_by_row(&self, row: usize) -> Option<Entity> {
        self.entities.get(row).copied()
    }

    /// Adds an entity and its components to this archetype.
    /// This is an internal method, typically called by the `Registry`.
    ///
    /// # Arguments
    /// * `entity` - The entity to add.
    /// * `components` - A vector of `Box<dyn Any>`, representing the component data for the entity.
    ///                  The order and types must match the archetype's `component_types`.
    ///
    /// # Returns
    /// The row index where the entity was added.
    ///
    /// # Panics
    /// Panics if the number of provided components does not match the number of component types
    /// in the archetype.
    /// Panics if `entity_to_row_map` already contains the entity.
    pub(crate) fn add_entity_components(
        &mut self,
        entity: Entity,
        components: Vec<Box<dyn Any>>,
    ) -> usize {
        if self.entity_to_row_map.contains_key(&entity) {
            panic!(
                "Entity {:?} already exists in Archetype {:?}",
                entity, self.id
            );
        }
        assert_eq!(
            components.len(),
            self.component_types.len(),
            "Incorrect number of components for Archetype {:?}. Expected {}, got {}.",
            self.id,
            self.component_types.len(),
            components.len()
        );

        for (i, component_data) in components.into_iter().enumerate() {
            // Ensure the provided component matches the expected type for this column
            // This check is implicitly handled by AnyStorage::push_new if it panics on mismatch,
            // but an explicit check against self.component_types[i] could be added here
            // if AnyStorage::push_new didn't already use the stored component_type_id for its panic message.
            self.component_storage[i].push_new(component_data);
        }

        let row_index = self.entities.len();
        self.entities.push(entity);
        self.entity_to_row_map.insert(entity, row_index);
        row_index
    }

    /// Adds an entity to this archetype without adding any component data.
    /// Returns the row index where the entity is stored.
    ///
    /// # Panics
    /// Panics if the entity already exists in this archetype.
    pub fn add_entity_placeholder(&mut self, entity: Entity) -> usize {
        if self.entity_to_row_map.contains_key(&entity) {
            panic!(
                "Entity {:?} already exists in Archetype {:?}.",
                entity, self.id
            );
        }

        // Add entity to the list of entities
        self.entities.push(entity);
        let new_row_index = self.entities.len() - 1;

        // Update the mapping from entity to its row
        self.entity_to_row_map.insert(entity, new_row_index);

        // For "null" archetypes (which have no component_types), component_storage will be empty,
        // so no further action is needed here regarding component columns.
        // This function's primary role is to register the entity's existence in this archetype's
        // entity list and map.
        new_row_index
    }

    /// Removes an entity and its components from this archetype using swap_remove.
    /// This is an internal method, typically called by the `Registry`.
    ///
    /// # Arguments
    /// * `entity_row` - The row index of the entity to remove.
    ///
    /// # Returns
    /// A tuple containing:
    ///   - The `Entity` that was removed.
    ///   - A `Vec<Box<dyn Any>>` of the removed components.
    ///   - An `Option<Entity>` which is the entity that was swapped into the removed entity's row, if any.
    ///
    /// # Panics
    /// Panics if `entity_row` is out of bounds.
    pub(crate) fn remove_entity_by_row(
        &mut self,
        entity_row: usize,
    ) -> (Entity, Vec<Box<dyn Any>>, Option<Entity>) {
        if entity_row >= self.entities.len() {
            panic!(
                "Entity row {} is out of bounds for Archetype {:?}",
                entity_row, self.id
            );
        }

        let removed_entity = self.entities.swap_remove(entity_row);
        self.entity_to_row_map.remove(&removed_entity);

        let mut removed_components = Vec::with_capacity(self.component_storage.len());
        for storage_column in self.component_storage.iter_mut() {
            if let Some(component_data) = storage_column.swap_remove(entity_row) {
                removed_components.push(component_data);
            } else {
                // This should not happen if entity_row is valid and storages are consistent
                panic!(
                    "Failed to remove component from column in Archetype {:?} at row {}. Storage len: {}",
                    self.id,
                    entity_row,
                    storage_column.len()
                );
            }
        }

        let swapped_in_entity = if entity_row < self.entities.len() {
            // Check if something was actually swapped in
            let entity_that_moved = self.entities[entity_row];
            self.entity_to_row_map.insert(entity_that_moved, entity_row); // Update its row
            Some(entity_that_moved)
        } else {
            None
        };

        (removed_entity, removed_components, swapped_in_entity)
    }

    // Helper to get a mutable reference to a specific storage column.
    // This would be used by the Registry or query system.
    // Unsafe because the caller must ensure the type T matches the storage.
    // A safer version would involve checking ComponentTypeId.
    pub(crate) unsafe fn get_storage_mut<T: Component>(
        &mut self,
        component_type_id: ComponentTypeId,
    ) -> Option<&mut Storage<T>> {
        if let Some(index) =
            self.component_types.iter().position(|ct_id| *ct_id == component_type_id)
        {
            // Additional safety: check if the storage at index actually matches T.
            // This relies on component_type_id() being correctly implemented by AnyStorage.
            if self.component_storage[index].component_type_id() == ComponentTypeId::of::<T>()
            {
                self.component_storage[index].as_any_mut().downcast_mut::<Storage<T>>()
            } else {
                None // Type mismatch
            }
        } else {
            None // Component type not found in archetype
        }
    }

    // Helper to get an immutable reference to a specific storage column.
    pub(crate) unsafe fn get_storage<T: Component>(
        &self,
        component_type_id: ComponentTypeId,
    ) -> Option<&Storage<T>> {
        if let Some(index) =
            self.component_types.iter().position(|ct_id| *ct_id == component_type_id)
        {
            if self.component_storage[index].component_type_id() == ComponentTypeId::of::<T>()
            {
                self.component_storage[index].as_any().downcast_ref::<Storage<T>>()
            } else {
                None
            }
        } else {
            None
        }
    }
}

#[cfg(test)]
mod tests {
    use void_architect_ecs_macros::Component;

    use super::*;
    // Ensure Component trait is in scope from the crate root or component module
    use crate::component::Component;

    // Define some simple components for testing
    #[derive(Component, Debug, PartialEq, Clone)]
    struct Position {
        x: f32,
        y: f32,
    }

    #[derive(Component, Debug, PartialEq, Clone)]
    struct Velocity {
        dx: f32,
        dy: f32,
    }

    #[derive(Component)]
    struct Tag; // A marker component

    // Helper to create entities for tests, assuming Entity::new is accessible
    // If Entity::new is pub(crate) in entity.rs, tests in this module can use it.
    fn new_entity_for_test(id: usize) -> Entity {
        Entity::new(id, 0)
    }

    #[test]
    fn archetype_id_creation_and_value() {
        let id_val: usize = 5;
        let arch_id = ArchetypeId::new(id_val);
        assert_eq!(arch_id.value(), id_val);
    }

    #[test]
    fn storage_new_push_get() {
        let mut pos_storage = Storage::<Position>::new();
        assert_eq!(pos_storage.len(), 0);
        assert!(pos_storage.is_empty());
        assert_eq!(
            pos_storage.component_type_id(),
            ComponentTypeId::of::<Position>()
        );

        let pos1 = Position { x: 1.0, y: 2.0 };
        // Use the AnyStorage trait method for testing type erasure aspects
        let mut any_storage: Box<dyn AnyStorage> = Box::new(pos_storage);
        any_storage.push_new(Box::new(pos1.clone()));

        assert_eq!(any_storage.len(), 1);
        assert!(!any_storage.is_empty());

        // Downcast back to get typed access for assertion
        let typed_storage = any_storage.as_any().downcast_ref::<Storage<Position>>().unwrap();
        assert_eq!(typed_storage.get(0), Some(&pos1));
    }

    #[test]
    fn storage_swap_remove() {
        let mut vel_storage = Storage::<Velocity>::new();
        let vel1 = Velocity { dx: 1.0, dy: 0.0 };
        let vel2 = Velocity { dx: 0.0, dy: 1.0 };

        let mut any_storage: Box<dyn AnyStorage> = Box::new(vel_storage);
        any_storage.push_new(Box::new(vel1.clone()));
        any_storage.push_new(Box::new(vel2.clone()));

        let removed_vel_any = any_storage.swap_remove(0).unwrap();
        let removed_vel = removed_vel_any.downcast::<Velocity>().unwrap();
        assert_eq!(*removed_vel, vel1);
        assert_eq!(any_storage.len(), 1);

        let typed_storage = any_storage.as_any().downcast_ref::<Storage<Velocity>>().unwrap();
        assert_eq!(typed_storage.get(0).unwrap(), &vel2); // vel2 should now be at index 0
    }

    #[test]
    #[should_panic(expected = "Mismatched component type pushed to Storage.")]
    fn storage_push_mismatched_type_panic() {
        let pos_storage = Storage::<Position>::new(); // mut removed
        let vel = Velocity { dx: 1.0, dy: 1.0 };
        let mut any_storage: Box<dyn AnyStorage> = Box::new(pos_storage);
        any_storage.push_new(Box::new(vel)); // Should panic
    }

    #[test]
    fn archetype_new_and_basic_properties() {
        let arch_id = ArchetypeId::new(0);
        let comp_types = vec![
            ComponentTypeId::of::<Position>(),
            ComponentTypeId::of::<Velocity>(),
        ];

        let mut storages: Vec<Box<dyn AnyStorage>> = Vec::new();
        storages.push(Box::new(Storage::<Position>::new()));
        storages.push(Box::new(Storage::<Velocity>::new()));

        // Clone comp_types for Archetype::new as it takes ownership and sorts it.
        // Keep the original comp_types for later comparison, but it needs to be sorted for that.
        let mut expected_comp_types = comp_types.clone();
        expected_comp_types.sort_unstable(); // Sort for correct comparison

        let archetype = Archetype::new(arch_id, comp_types, storages);

        assert_eq!(archetype.id(), arch_id);
        assert_eq!(archetype.component_types(), &expected_comp_types[..]); // Compare with the sorted version
        assert!(archetype.has_component_type(&ComponentTypeId::of::<Position>()));
        assert!(!archetype.has_component_type(&ComponentTypeId::of::<Tag>()));
        assert_eq!(archetype.entities_count(), 0);
    }

    #[test]
    #[should_panic(expected = "Mismatch between component types and storages count")]
    fn archetype_new_mismatched_storages_count_panic() {
        let arch_id = ArchetypeId::new(0);
        let comp_types = vec![
            ComponentTypeId::of::<Position>(),
            ComponentTypeId::of::<Velocity>(),
        ];
        let mut storages: Vec<Box<dyn AnyStorage>> = Vec::new();
        storages.push(Box::new(Storage::<Position>::new()));
        // Missing Velocity storage
        Archetype::new(arch_id, comp_types.clone(), storages);
    }

    #[test]
    #[should_panic(expected = "Storage at index 0 in Archetype")]
    fn archetype_new_storage_type_mismatch_panic() {
        let arch_id = ArchetypeId::new(0);
        let comp_types = vec![ComponentTypeId::of::<Position>()]; // Expects Position
        let mut storages: Vec<Box<dyn AnyStorage>> = Vec::new();
        storages.push(Box::new(Storage::<Velocity>::new())); // Provides Velocity

        Archetype::new(arch_id, comp_types.clone(), storages);
    }

    #[test]
    fn archetype_add_entity_and_components() {
        let arch_id = ArchetypeId::new(0);
        let comp_types = vec![
            ComponentTypeId::of::<Position>(),
            ComponentTypeId::of::<Velocity>(),
        ];
        let mut storages_init: Vec<Box<dyn AnyStorage>> = Vec::new();
        storages_init.push(Box::new(Storage::<Position>::new()));
        storages_init.push(Box::new(Storage::<Velocity>::new()));
        let mut archetype = Archetype::new(arch_id, comp_types.clone(), storages_init);

        let entity1 = new_entity_for_test(1);
        let pos1 = Position { x: 1.0, y: 1.0 };
        let vel1 = Velocity { dx: 0.1, dy: 0.1 };

        // Prepare components with their TypeIds to sort them according to the archetype's internal order
        let mut components_with_ids1: Vec<(ComponentTypeId, Box<dyn Any>)> = vec![
            (ComponentTypeId::of::<Position>(), Box::new(pos1.clone())),
            (ComponentTypeId::of::<Velocity>(), Box::new(vel1.clone())),
        ];
        components_with_ids1.sort_unstable_by_key(|(type_id, _)| *type_id);
        let sorted_components1: Vec<Box<dyn Any>> =
            components_with_ids1.into_iter().map(|(_, comp)| comp).collect();

        let row1 = archetype.add_entity_components(entity1, sorted_components1);
        assert_eq!(row1, 0);
        assert_eq!(archetype.entities_count(), 1);
        assert_eq!(archetype.entities(), &[entity1]);
        assert_eq!(archetype.get_entity_row(entity1), Some(0));

        unsafe {
            let pos_storage =
                archetype.get_storage::<Position>(ComponentTypeId::of::<Position>()).unwrap();
            assert_eq!(pos_storage.get(0), Some(&pos1));
            let vel_storage =
                archetype.get_storage::<Velocity>(ComponentTypeId::of::<Velocity>()).unwrap();
            assert_eq!(vel_storage.get(0), Some(&vel1));
        }

        // Add second entity
        let entity2 = new_entity_for_test(2);
        let pos2 = Position { x: 2.0, y: 2.0 };
        let vel2 = Velocity { dx: 0.2, dy: 0.2 };

        let mut components_with_ids2: Vec<(ComponentTypeId, Box<dyn Any>)> = vec![
            (ComponentTypeId::of::<Position>(), Box::new(pos2.clone())),
            (ComponentTypeId::of::<Velocity>(), Box::new(vel2.clone())),
        ];
        components_with_ids2.sort_unstable_by_key(|(type_id, _)| *type_id);
        let sorted_components2: Vec<Box<dyn Any>> =
            components_with_ids2.into_iter().map(|(_, comp)| comp).collect();

        let row2 = archetype.add_entity_components(entity2, sorted_components2);
        assert_eq!(row2, 1); // Should be added at the next available row
        assert_eq!(archetype.entities_count(), 2);
        assert_eq!(archetype.get_entity_row(entity2), Some(1));
        assert_eq!(archetype.entities(), &[entity1, entity2]);

        unsafe {
            let pos_storage =
                archetype.get_storage::<Position>(ComponentTypeId::of::<Position>()).unwrap();
            assert_eq!(pos_storage.get(row2), Some(&pos2));
            let vel_storage =
                archetype.get_storage::<Velocity>(ComponentTypeId::of::<Velocity>()).unwrap();
            assert_eq!(vel_storage.get(row2), Some(&vel2));
        }
    }

    #[test]
    #[should_panic(expected = "Entity ")] // Panic message from add_entity_components
    fn archetype_add_entity_twice_panics() {
        let arch_id = ArchetypeId::new(0);
        let comp_types = vec![ComponentTypeId::of::<Position>()];
        let mut storages_init: Vec<Box<dyn AnyStorage>> = Vec::new();
        storages_init.push(Box::new(Storage::<Position>::new()));
        let mut archetype = Archetype::new(arch_id, comp_types.clone(), storages_init);

        let entity1 = new_entity_for_test(1);
        let pos1 = Position { x: 1.0, y: 1.0 };
        // For a single component, sorting is trivial but good practice for consistency
        let components_for_first_call: Vec<Box<dyn Any>> = vec![Box::new(pos1.clone())];
        archetype.add_entity_components(entity1, components_for_first_call);
        // Try to add the same entity again
        let components_for_second_call: Vec<Box<dyn Any>> = vec![Box::new(pos1.clone())];
        archetype.add_entity_components(entity1, components_for_second_call);
    }

    #[test]
    fn archetype_remove_entity_by_row() {
        let arch_id = ArchetypeId::new(0);
        let comp_types = vec![ComponentTypeId::of::<Position>()];
        let mut storages_init: Vec<Box<dyn AnyStorage>> = Vec::new();
        storages_init.push(Box::new(Storage::<Position>::new()));
        let mut archetype = Archetype::new(arch_id, comp_types.clone(), storages_init);

        let entity1 = new_entity_for_test(1);
        let pos1 = Position { x: 1.0, y: 1.0 };
        archetype.add_entity_components(entity1, vec![Box::new(pos1.clone())]);

        let entity2 = new_entity_for_test(2);
        let pos2 = Position { x: 2.0, y: 2.0 };
        archetype.add_entity_components(entity2, vec![Box::new(pos2.clone())]);

        assert_eq!(archetype.entities_count(), 2);

        // Remove entity1 (at row 0)
        let (removed_e, removed_c, swapped_e) = archetype.remove_entity_by_row(0);
        assert_eq!(removed_e, entity1);
        assert_eq!(removed_c.len(), 1);
        assert_eq!(*removed_c[0].downcast_ref::<Position>().unwrap(), pos1);
        assert_eq!(swapped_e, Some(entity2)); // entity2 was swapped into row 0

        assert_eq!(archetype.entities_count(), 1);
        assert_eq!(archetype.entities(), &[entity2]); // Only entity2 remains
        assert_eq!(archetype.get_entity_row(entity2), Some(0)); // entity2 is now at row 0
        assert_eq!(archetype.get_entity_row(entity1), None);

        unsafe {
            let pos_storage =
                archetype.get_storage::<Position>(ComponentTypeId::of::<Position>()).unwrap();
            assert_eq!(pos_storage.len(), 1);
            assert_eq!(pos_storage.get(0), Some(&pos2)); // pos2 should be at index 0
        }
    }

    #[test]
    fn archetype_get_storage_mut_and_get_storage() {
        let arch_id = ArchetypeId::new(0);
        let comp_types = vec![
            ComponentTypeId::of::<Position>(),
            ComponentTypeId::of::<Velocity>(),
        ];
        let mut storages_init: Vec<Box<dyn AnyStorage>> = Vec::new();
        storages_init.push(Box::new(Storage::<Position>::new()));
        storages_init.push(Box::new(Storage::<Velocity>::new()));
        let mut archetype = Archetype::new(arch_id, comp_types.clone(), storages_init);

        let entity1 = new_entity_for_test(1);
        let pos1 = Position { x: 1.0, y: 1.0 };
        let vel1 = Velocity { dx: 0.1, dy: 0.1 };

        let mut components_with_ids: Vec<(ComponentTypeId, Box<dyn Any>)> = vec![
            (ComponentTypeId::of::<Position>(), Box::new(pos1.clone())),
            (ComponentTypeId::of::<Velocity>(), Box::new(vel1.clone())),
        ];
        components_with_ids.sort_unstable_by_key(|(type_id, _)| *type_id);
        let sorted_components: Vec<Box<dyn Any>> =
            components_with_ids.into_iter().map(|(_, comp)| comp).collect();

        archetype.add_entity_components(entity1, sorted_components);

        // Test get_storage (immutable)
        unsafe {
            let pos_storage_ref =
                archetype.get_storage::<Position>(ComponentTypeId::of::<Position>()).unwrap();
            assert_eq!(pos_storage_ref.get(0), Some(&pos1));

            let vel_storage_ref =
                archetype.get_storage::<Velocity>(ComponentTypeId::of::<Velocity>()).unwrap();
            assert_eq!(vel_storage_ref.get(0), Some(&vel1));

            // Try to get a non-existent component type
            assert!(archetype.get_storage::<Tag>(ComponentTypeId::of::<Tag>()).is_none());
            // Try to get with wrong type
            assert!(archetype.get_storage::<Tag>(ComponentTypeId::of::<Position>()).is_none());
        }

        // Test get_storage_mut (mutable)
        unsafe {
            let pos_storage_mut = archetype
                .get_storage_mut::<Position>(ComponentTypeId::of::<Position>())
                .unwrap();
            pos_storage_mut.get_mut(0).unwrap().x = 10.0;

            let vel_storage_mut = archetype
                .get_storage_mut::<Velocity>(ComponentTypeId::of::<Velocity>())
                .unwrap();
            vel_storage_mut.get_mut(0).unwrap().dx = 100.0;

            // Verify changes
            let updated_pos1 = Position { x: 10.0, y: 1.0 };
            let updated_vel1 = Velocity { dx: 100.0, dy: 0.1 };

            let pos_storage_ref_after_mut =
                archetype.get_storage::<Position>(ComponentTypeId::of::<Position>()).unwrap();
            assert_eq!(pos_storage_ref_after_mut.get(0), Some(&updated_pos1));

            let vel_storage_ref_after_mut =
                archetype.get_storage::<Velocity>(ComponentTypeId::of::<Velocity>()).unwrap();
            assert_eq!(vel_storage_ref_after_mut.get(0), Some(&updated_vel1));

            // Try to get_mut a non-existent component type
            assert!(archetype.get_storage_mut::<Tag>(ComponentTypeId::of::<Tag>()).is_none());
            // Try to get_mut with wrong type
            assert!(
                archetype.get_storage_mut::<Tag>(ComponentTypeId::of::<Position>()).is_none()
            );
        }
    }
}
