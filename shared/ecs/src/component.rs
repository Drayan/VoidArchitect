use std::any::{Any, TypeId};
use std::collections::{BTreeSet, HashMap};
use void_architect_ecs_macros::impl_component_bundle_for_tuples;

pub trait Component: 'static + Send + Sync {}

#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct ComponentTypeId(TypeId);

impl ComponentTypeId {
    pub fn of<T: Component>() -> Self {
        Self(TypeId::of::<T>())
    }
}

pub trait ComponentBundle: Send + Sync {
    /// Returns a sorted and unique set of `ComponentTypeId`s for the components in this bundle.
    fn get_component_type_ids(&self) -> BTreeSet<ComponentTypeId>;

    /// Consumes the bundle and returns a map of `ComponentTypeId` to boxed component data.
    fn into_components_data(self) -> HashMap<ComponentTypeId, Box<dyn Any>>;
}

// Generate implementations for tuples
impl_component_bundle_for_tuples!();
