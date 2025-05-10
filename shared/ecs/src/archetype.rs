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

    /// Replaces the component at a given row index with new data.
    /// The caller must ensure `component_data` is of the correct type for this storage.
    /// Panics if `row_index` is out of bounds or if `component_data` is of the wrong type.
    fn replace_at_row(&mut self, row_index: usize, component_data: Box<dyn Any>);

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

    fn replace_at_row(&mut self, row_index: usize, component_data: Box<dyn Any>) {
        if row_index >= self.components.len() {
            panic!(
                "replace_at_row: row_index {} is out of bounds for len {}",
                row_index,
                self.components.len()
            );
        }
        match component_data.downcast::<T>() {
            Ok(component) => {
                self.components[row_index] = *component;
            }
            Err(_) => panic!(
                "Mismatched component type in replace_at_row. Expected type: {:?}, found different.",
                self.component_type_id
            ),
        }
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

    /// Retrieves a read-only reference to a component of type `C` for an entity at a specific `row_index`.
    ///
    /// This method is intended for internal use by the `Registry`.
    ///
    /// # Arguments
    /// * `row_index` - The row index of the entity within this archetype.
    ///
    /// # Returns
    /// `Some(&C)` if the component `C` exists for the entity at `row_index` and is part of this archetype.
    /// `None` if component `C` is not part of this archetype's signature, or if `row_index` is out of bounds.
    pub(crate) fn get_component_at_row<C: Component>(&self, row_index: usize) -> Option<&C> {
        let component_type_id = ComponentTypeId::of::<C>();
        match self.component_types.binary_search(&component_type_id) {
            Ok(column_idx) => {
                // Found the component type in this archetype's signature.
                // Now, get the specific storage column.
                self.component_storage.get(column_idx).and_then(|any_storage_box| {
                    // Downcast the type-erased storage to the concrete Storage<C>.
                    any_storage_box.as_any().downcast_ref::<Storage<C>>().and_then(
                        |storage_c| {
                            // Get the component from the typed storage at the given row.
                            storage_c.get(row_index)
                        },
                    )
                })
            }
            Err(_) => {
                // Component C is not part of this archetype's signature.
                None
            }
        }
    }

    /// Retrieves a mutable reference to a component of type `C` for an entity at a specific `row_index`.
    ///
    /// This method is intended for internal use by the `Registry`.
    ///
    /// # Arguments
    /// * `row_index` - The row index of the entity within this archetype.
    ///
    /// # Returns
    /// `Some(&mut C)` if component `C` exists for the entity at `row_index` and is part of this archetype.
    /// `None` if component `C` is not part of this archetype's signature, or if `row_index` is out of bounds.
    pub(crate) fn get_component_at_row_mut<C: Component>(
        &mut self,
        row_index: usize,
    ) -> Option<&mut C> {
        let component_type_id = ComponentTypeId::of::<C>();
        match self.component_types.binary_search(&component_type_id) {
            Ok(column_idx) => {
                // Found the component type in this archetype's signature.
                // Now, get the specific storage column mutably.
                self.component_storage.get_mut(column_idx).and_then(|any_storage_box| {
                    // Downcast the type-erased storage to the concrete Storage<C> mutably.
                    any_storage_box.as_any_mut().downcast_mut::<Storage<C>>().and_then(
                        |storage_c| {
                            // Get the component mutably from the typed storage at the given row.
                            storage_c.get_mut(row_index)
                        },
                    )
                })
            }
            Err(_) => {
                // Component C is not part of this archetype's signature.
                None
            }
        }
    }

    /// Returns the unique ID of this archetype.
    pub fn id(&self) -> ArchetypeId {
        self.id
    }

    /// Returns a clone of the set of `ComponentTypeId`s that define this archetype's signature.
    pub fn signature(&self) -> std::collections::BTreeSet<ComponentTypeId> {
        self.component_types.iter().cloned().collect()
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

    /// Returns a reference to the type-erased storage for a given component type, if it exists in this archetype.
    /// This is intended for internal use by query logic or advanced operations.
    pub(crate) fn get_storage_by_id(
        &self,
        component_type_id: ComponentTypeId,
    ) -> Option<&dyn AnyStorage> {
        match self.component_types.binary_search(&component_type_id) {
            Ok(idx) => self.component_storage.get(idx).map(|boxed_storage| &**boxed_storage),
            Err(_) => None,
        }
    }

    /// Returns a mutable reference to the type-erased storage for a given component type, if it exists in this archetype.
    /// This is intended for internal use by query logic or advanced operations.
    pub(crate) fn get_storage_by_id_mut(
        &mut self,
        component_type_id: ComponentTypeId,
    ) -> Option<&mut dyn AnyStorage> {
        match self.component_types.binary_search(&component_type_id) {
            Ok(idx) => {
                self.component_storage.get_mut(idx).map(|boxed_storage| &mut **boxed_storage)
            }
            Err(_) => None,
        }
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
    #[allow(dead_code)] // Used by Registry and/or tests
    pub(crate) fn get_entity_row(&self, entity: Entity) -> Option<usize> {
        self.entity_to_row_map.get(&entity).copied()
    }

    #[allow(dead_code)] // Used by Registry and/or tests
    pub(crate) fn get_entity_by_row(&self, row: usize) -> Option<Entity> {
        self.entities.get(row).copied()
    }

    /// Updates a component for an entity at a specific row in this archetype.
    /// This is used when an entity already in this archetype has one of its components updated.
    ///
    /// # Arguments
    /// * `row_index` - The row index of the entity whose component is to be updated.
    /// * `component_type_id` - The `ComponentTypeId` of the component to update.
    /// * `component_data` - A `Box<dyn Any>` containing the new data for the component.
    ///
    /// # Panics
    /// * If `row_index` is out of bounds.
    /// * If `component_type_id` is not part of this archetype's signature.
    /// * If `component_data` cannot be downcast to the expected type for the storage.
    pub(crate) fn update_component_at_row(
        &mut self,
        row_index: usize,
        component_type_id: ComponentTypeId,
        component_data: Box<dyn Any>,
    ) {
        if row_index >= self.entities.len() {
            panic!(
                "update_component_at_row: row_index {} is out of bounds for Archetype {:?} with {} entities.",
                row_index,
                self.id,
                self.entities.len()
            );
        }

        match self.component_types.binary_search(&component_type_id) {
            Ok(storage_idx) => {
                self.component_storage[storage_idx].replace_at_row(row_index, component_data);
            }
            Err(_) => {
                panic!(
                    "Attempted to update component {:?} at row {} which is not part of Archetype {:?}'s signature.",
                    component_type_id, row_index, self.id
                );
            }
        }
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
    #[allow(dead_code)] // Used by Registry and/or tests
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

    /// Adds an entity along with all its components, provided as a map.
    /// This is used when migrating an entity to this archetype.
    ///
    /// # Arguments
    /// * `entity` - The `Entity` to add.
    /// * `all_components_data` - A mutable reference to a `HashMap` where keys are `ComponentTypeId`
    ///   and values are `Box<dyn Any>`. This map should contain data for all component types
    ///   defined by this archetype's signature. Components are removed from the map as they are used.
    ///
    /// # Returns
    /// The row index where the entity was added.
    ///
    /// # Panics
    /// * If `entity` already exists in this archetype.
    /// * If `all_components_data` is missing an entry for any `ComponentTypeId` in this archetype's signature.
    pub(crate) fn add_entity_with_all_components(
        &mut self,
        entity: Entity,
        all_components_data: &mut HashMap<ComponentTypeId, Box<dyn Any>>,
    ) -> usize {
        if self.entity_to_row_map.contains_key(&entity) {
            panic!(
                "Entity {:?} already exists in Archetype {:?}",
                entity, self.id
            );
        }

        let new_row_index = self.entities.len();

        for (storage_idx, component_type_id) in self.component_types.iter().enumerate() {
            if let Some(component_data) = all_components_data.remove(component_type_id) {
                self.component_storage[storage_idx].push_new(component_data);
            } else {
                panic!(
                    "Missing component data for type {:?} (required by Archetype {:?}) when adding entity {:?}.",
                    component_type_id, self.id, entity
                );
            }
        }
        // Note: `all_components_data` might still contain components not part of this archetype's
        // signature. This is acceptable if the map was a superset (e.g., from a previous archetype).
        // The `Registry` is responsible for ensuring only relevant data is passed or handled.

        self.entities.push(entity);
        self.entity_to_row_map.insert(entity, new_row_index);
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
    ///   - A `HashMap<ComponentTypeId, Box<dyn Any>>` of the removed entity's components.
    ///   - An `Option<Entity>` which is the entity that was swapped into the removed entity's row, if any.
    ///
    /// # Panics
    /// Panics if `entity_row` is out of bounds.
    pub(crate) fn remove_entity_by_row(
        &mut self,
        entity_row: usize,
    ) -> (
        Entity,
        HashMap<ComponentTypeId, Box<dyn Any>>,
        Option<Entity>,
    ) {
        if entity_row >= self.entities.len() {
            panic!(
                "Entity row {} is out of bounds for Archetype {:?}",
                entity_row, self.id
            );
        }

        let removed_entity = self.entities.swap_remove(entity_row);
        self.entity_to_row_map.remove(&removed_entity);

        let mut removed_components_map = HashMap::with_capacity(self.component_storage.len());
        for (i, component_type_id) in self.component_types.iter().enumerate() {
            // The component_storage is parallel to component_types
            let storage_column = &mut self.component_storage[i];
            if let Some(component_data) = storage_column.swap_remove(entity_row) {
                removed_components_map.insert(*component_type_id, component_data);
            } else {
                // This should not happen if entity_row is valid and storages are consistent
                panic!(
                    "Failed to remove component {:?} from column in Archetype {:?} at row {}. Storage len: {}",
                    component_type_id,
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

        (removed_entity, removed_components_map, swapped_in_entity)
    }
}

#[cfg(test)]
mod tests {
    // Made public for Tag and other test components to be accessible by registry tests
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

    #[derive(Component, Debug, Clone, PartialEq)]
    struct Tag; // A marker component, made public for registry tests

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
        let pos_storage = Storage::<Position>::new(); // Removed mut
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
        let vel_storage = Storage::<Velocity>::new(); // Removed mut
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

        // Verify components using get_component_at_row
        // Original unsafe block was here, checking pos_storage.len() and vel_storage.len() too.
        // Those checks are implicitly covered by entities_count() and successful component retrieval.
        assert_eq!(
            archetype.get_component_at_row::<Position>(row1),
            Some(&pos1)
        );
        assert_eq!(
            archetype.get_component_at_row::<Velocity>(row1),
            Some(&vel1)
        );

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

        // Verify components for the second entity using get_component_at_row
        // Original unsafe block was here, checking pos_storage.len() and vel_storage.len() too.
        // Assertions for entity1's components (still at row1) can also be added if desired,
        // but the main point here is to check entity2 at row2.
        assert_eq!(
            archetype.get_component_at_row::<Position>(row1),
            Some(&pos1)
        ); // Check entity1 still OK
        assert_eq!(
            archetype.get_component_at_row::<Velocity>(row1),
            Some(&vel1)
        ); // Check entity1 still OK
        assert_eq!(
            archetype.get_component_at_row::<Position>(row2),
            Some(&pos2)
        );
        assert_eq!(
            archetype.get_component_at_row::<Velocity>(row2),
            Some(&vel2)
        );
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
        assert_eq!(
            *removed_c
                .get(&ComponentTypeId::of::<Position>())
                .unwrap()
                .downcast_ref::<Position>()
                .unwrap(),
            pos1
        );
        assert_eq!(swapped_e, Some(entity2)); // entity2 was swapped into row 0

        assert_eq!(archetype.entities_count(), 1);
        assert_eq!(archetype.entities(), &[entity2]); // Only entity2 remains
        assert_eq!(archetype.get_entity_row(entity2), Some(0)); // entity2 is now at row 0
        assert_eq!(archetype.get_entity_row(entity1), None);

        // Verify remaining component for entity2 (now at row 0)
        assert_eq!(archetype.entities_count(), 1);
        assert_eq!(archetype.get_component_at_row::<Position>(0), Some(&pos2)); // entity2 (pos2) is now at row 0
    }

    /*
    #[test] // Temporarily commented out as it uses get_storage and get_storage_mut which are to be removed
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
    */

    #[test]
    fn archetype_get_component_at_row() {
        // Setup: Create an archetype with Position and Velocity components
        let arch_id = ArchetypeId::new(0);
        let pos_type_id = ComponentTypeId::of::<Position>();
        let vel_type_id = ComponentTypeId::of::<Velocity>();

        let storages: Vec<Box<dyn AnyStorage>> = vec![
            // Removed mut
            Box::new(Storage::<Position>::new()),
            Box::new(Storage::<Velocity>::new()),
        ];
        let component_types = vec![pos_type_id, vel_type_id]; // Will be sorted by Archetype::new

        let mut archetype = Archetype::new(arch_id, component_types, storages);

        // Add an entity with components
        let entity1 = new_entity_for_test(1);
        let pos1 = Position { x: 1.0, y: 2.0 };
        let vel1 = Velocity { dx: 3.0, dy: 4.0 };
        let mut components_map1 = HashMap::new();
        components_map1.insert(pos_type_id, Box::new(pos1.clone()) as Box<dyn Any>);
        components_map1.insert(vel_type_id, Box::new(vel1.clone()) as Box<dyn Any>);

        // Convert HashMap to Vec<Box<dyn Any>> sorted by archetype's component_types
        let sorted_components1: Vec<Box<dyn Any>> = archetype
            .component_types() // Assuming component_types() getter exists and returns sorted Vec<ComponentTypeId>
            .iter()
            .map(|ctid| {
                components_map1
                    .remove(ctid)
                    .expect("Component for type ID should exist in map")
            })
            .collect();
        let row1 = archetype.add_entity_components(entity1, sorted_components1);

        // Test: Get Position component for entity1 at row1
        let retrieved_pos = archetype.get_component_at_row::<Position>(row1);
        assert_eq!(retrieved_pos, Some(&pos1));

        // Test: Get Velocity component for entity1 at row1
        let retrieved_vel = archetype.get_component_at_row::<Velocity>(row1);
        assert_eq!(retrieved_vel, Some(&vel1));

        // Test: Get component at invalid row index
        let invalid_row_pos = archetype.get_component_at_row::<Position>(row1 + 1);
        assert!(invalid_row_pos.is_none());

        // Test: Get component type not in archetype (e.g., Tag)
        #[derive(Component, Debug, PartialEq)]
        struct Tag;
        let non_existent_comp = archetype.get_component_at_row::<Tag>(row1);
        assert!(non_existent_comp.is_none());

        // Add another entity
        let entity2 = new_entity_for_test(2);
        let pos2 = Position { x: 10.0, y: 20.0 };
        let vel2 = Velocity { dx: 30.0, dy: 40.0 };
        let mut components_map2 = HashMap::new();
        components_map2.insert(pos_type_id, Box::new(pos2.clone()) as Box<dyn Any>);
        components_map2.insert(vel_type_id, Box::new(vel2.clone()) as Box<dyn Any>);

        // Convert HashMap to Vec<Box<dyn Any>> sorted by archetype's component_types
        let sorted_components2: Vec<Box<dyn Any>> = archetype
            .component_types()
            .iter()
            .map(|ctid| {
                components_map2
                    .remove(ctid)
                    .expect("Component for type ID should exist in map")
            })
            .collect();
        let row2 = archetype.add_entity_components(entity2, sorted_components2);

        assert_eq!(
            archetype.get_component_at_row::<Position>(row2),
            Some(&pos2)
        );
        assert_eq!(
            archetype.get_component_at_row::<Velocity>(row2),
            Some(&vel2)
        );
    }

    #[test]
    fn archetype_get_component_at_row_mut() {
        let arch_id = ArchetypeId::new(0);
        let pos_type_id = ComponentTypeId::of::<Position>();
        let vel_type_id = ComponentTypeId::of::<Velocity>();

        let storages: Vec<Box<dyn AnyStorage>> = vec![
            Box::new(Storage::<Position>::new()),
            Box::new(Storage::<Velocity>::new()),
        ];
        let component_types = vec![pos_type_id, vel_type_id];

        let mut archetype = Archetype::new(arch_id, component_types, storages);

        let entity1 = new_entity_for_test(1);
        let pos1 = Position { x: 1.0, y: 2.0 };
        let vel1 = Velocity { dx: 3.0, dy: 4.0 };
        let mut components_map1 = HashMap::new();
        components_map1.insert(pos_type_id, Box::new(pos1.clone()) as Box<dyn Any>);
        components_map1.insert(vel_type_id, Box::new(vel1.clone()) as Box<dyn Any>);

        // Convert HashMap to Vec<Box<dyn Any>> sorted by archetype's component_types
        let sorted_components1_mut: Vec<Box<dyn Any>> = archetype // Renamed to avoid conflict if in same scope
            .component_types()
            .iter()
            .map(|ctid| {
                components_map1
                    .remove(ctid)
                    .expect("Component for type ID should exist in map")
            })
            .collect();
        let row1 = archetype.add_entity_components(entity1, sorted_components1_mut);

        // Test: Get and modify Position component
        if let Some(retrieved_pos_mut) = archetype.get_component_at_row_mut::<Position>(row1) {
            retrieved_pos_mut.x = 100.0;
        } else {
            panic!("Failed to get mutable Position component");
        }
        let expected_pos_modified = Position { x: 100.0, y: 2.0 };
        assert_eq!(
            archetype.get_component_at_row::<Position>(row1),
            Some(&expected_pos_modified)
        );

        // Test: Get and modify Velocity component
        if let Some(retrieved_vel_mut) = archetype.get_component_at_row_mut::<Velocity>(row1) {
            retrieved_vel_mut.dy = 400.0;
        } else {
            panic!("Failed to get mutable Velocity component");
        }
        let expected_vel_modified = Velocity { dx: 3.0, dy: 400.0 };
        assert_eq!(
            archetype.get_component_at_row::<Velocity>(row1),
            Some(&expected_vel_modified)
        );

        // Test: Get mutable component at invalid row index
        let invalid_row_pos_mut = archetype.get_component_at_row_mut::<Position>(row1 + 1);
        assert!(invalid_row_pos_mut.is_none());

        // Test: Get mutable component type not in archetype (e.g., Tag)
        #[derive(Component, Debug, PartialEq)]
        struct Tag;
        let non_existent_comp_mut = archetype.get_component_at_row_mut::<Tag>(row1);
        assert!(non_existent_comp_mut.is_none());
    }
}
