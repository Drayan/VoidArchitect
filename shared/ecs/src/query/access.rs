use crate::component::ComponentTypeId;
use crate::query::registry_query::QueryParam; // Import QueryParam
use std::collections::HashMap; // For validation logic

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum AccessType {
    Read,
    Write,
}

#[derive(Debug, Clone, Copy)]
pub struct ComponentAccessInfo {
    pub component_id: ComponentTypeId,
    pub access_type: AccessType,
}

// Describes the set of component accesses for a query
pub struct QueryDescriptor {
    pub components: Vec<ComponentAccessInfo>,
    pub requests_entity_id: bool,
    // Potentially other metadata like "Added<T>", "Changed<T>" in the future
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum QueryValidationError {
    ConflictingAccess(ComponentTypeId),
    DuplicateMutableAccess(ComponentTypeId),
}

impl QueryDescriptor {
    /// Creates a new `QueryDescriptor` from a `QueryParam` type `Q`.
    /// This function collects access information from `Q` and validates it.
    pub fn new<Q: QueryParam>() -> Result<Self, QueryValidationError> {
        let mut components_access_info = Vec::new();
        Q::add_access(&mut components_access_info);
        let requests_entity_id = Q::requests_entity_id();

        // Validate component accesses
        // Use a HashMap to detect conflicting accesses for the same ComponentTypeId
        let mut access_map: HashMap<ComponentTypeId, AccessType> = HashMap::new();

        for access_info in &components_access_info {
            match access_map.entry(access_info.component_id) {
                std::collections::hash_map::Entry::Occupied(entry) => {
                    let existing_access = *entry.get();
                    // Conflict if one is Read and other is Write, or if both are Write.
                    if existing_access == AccessType::Write
                        || access_info.access_type == AccessType::Write
                    {
                        // If existing is Write, any new access is a conflict (duplicate mutable or read+write).
                        // If new is Write, and existing was Read, it's a conflict.
                        // If new is Write, and existing was Write, it's a duplicate mutable.
                        if existing_access == AccessType::Write
                            && access_info.access_type == AccessType::Write
                        {
                            return Err(QueryValidationError::DuplicateMutableAccess(
                                access_info.component_id,
                            ));
                        }
                        return Err(QueryValidationError::ConflictingAccess(
                            access_info.component_id,
                        ));
                    }
                    // If both are Read, it's fine (though redundant, QueryParam impls for tuples should handle this).
                }
                std::collections::hash_map::Entry::Vacant(entry) => {
                    entry.insert(access_info.access_type);
                }
            }
        }

        Ok(Self {
            components: components_access_info,
            requests_entity_id,
        })
    }
}
