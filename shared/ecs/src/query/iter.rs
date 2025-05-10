use super::access::QueryDescriptor;
use super::fetch::Fetch; // Add Fetch trait
use super::registry_query::QueryParam; // Adjusted path
use crate::archetype::{Archetype, ArchetypeId};
use crate::registry::Registry;

use std::marker::PhantomData;

pub struct QueryIter<'registry, 'state, Q: QueryParam> {
    registry: &'registry Registry,
    descriptor: QueryDescriptor, // Built from Q or passed in
    matching_archetypes: Vec<(&'registry Archetype, ArchetypeId)>, // Pre-filtered list of archetypes

    current_archetype_iter_idx: usize, // Index into matching_archetypes vector
    current_entity_idx_in_archetype: usize, // Index of entity within the current archetype

    // PhantomData to correctly use the 'state and Q generic parameters
    _phantom_query_state: PhantomData<&'state ()>,
    _phantom_query_type: PhantomData<Q>,
}

impl<'registry, 'state, Q: QueryParam> QueryIter<'registry, 'state, Q> {
    pub(crate) fn new(
        registry: &'registry Registry, /*, descriptor: QueryDescriptor */
    ) -> Self {
        // 1. Create QueryDescriptor from Q, which includes validation.
        let descriptor = match QueryDescriptor::new::<Q>() {
            Ok(desc) => desc,
            Err(validation_error) => {
                // For now, panic on invalid query.
                // In a more user-facing API, this might return a Result.
                panic!("Invalid query: {:?}", validation_error);
            }
        };

        // 2. Filter registry.archetypes() based on the descriptor.
        //    Iterate through all archetypes in the registry.
        //    For each archetype, check if it contains all components specified in
        //    `descriptor.components`.
        //    Note: AccessType compatibility (Read/Write) is handled by QueryDescriptor validation.
        //    Here, we just check for presence.
        let matching_archetypes = registry
            .archetypes
            .iter()
            .filter_map(|(id, archetype)| {
                let matches = descriptor.components.iter().all(|access_info| {
                    archetype.has_component_type(&access_info.component_id) // Corrected method name
                });
                if matches {
                    Some((archetype, *id))
                } else {
                    None
                }
            })
            .collect::<Vec<_>>();

        // Initialize Fetch for the first archetype if it exists
        if let Some((first_archetype, _)) = matching_archetypes.first() {
            Q::Fetch::init_archetype(first_archetype);
        }

        Self {
            registry,
            descriptor,
            matching_archetypes,
            current_archetype_iter_idx: 0,
            current_entity_idx_in_archetype: 0,
            _phantom_query_state: PhantomData,
            _phantom_query_type: PhantomData,
        }
    }
}

impl<'registry, 'state, Q: QueryParam + 'registry> Iterator
    for QueryIter<'registry, 'state, Q>
{
    type Item = Q::Item<'registry, 'state>;

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            if self.current_archetype_iter_idx >= self.matching_archetypes.len() {
                // No more archetypes to process
                return None;
            }

            let (current_archetype, _archetype_id) =
                self.matching_archetypes[self.current_archetype_iter_idx];

            if self.current_entity_idx_in_archetype < current_archetype.entities_count() {
                // Fetch data for the current entity in the current archetype
                let item = unsafe {
                    // This is unsafe because Fetch::fetch operates with raw pointers
                    // and assumes the context (archetype, entity_index) is correct.
                    Q::Fetch::fetch(current_archetype, self.current_entity_idx_in_archetype)
                };
                self.current_entity_idx_in_archetype += 1;
                return Some(item);
            } else {
                // Move to the next archetype
                self.current_archetype_iter_idx += 1;
                self.current_entity_idx_in_archetype = 0;

                // Initialize Fetch for the new archetype if it exists
                if self.current_archetype_iter_idx < self.matching_archetypes.len() {
                    let (next_archetype, _) =
                        self.matching_archetypes[self.current_archetype_iter_idx];
                    Q::Fetch::init_archetype(next_archetype);
                }
                // Continue loop to process the new archetype or end if no more archetypes
            }
        }
    }
}
