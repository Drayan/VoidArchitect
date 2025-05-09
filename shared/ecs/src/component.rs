use std::any::TypeId;

pub trait Component: 'static + Send + Sync {}

#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct ComponentTypeId(TypeId);

impl ComponentTypeId {
    pub fn of<T: Component>() -> Self {
        Self(TypeId::of::<T>())
    }
}
