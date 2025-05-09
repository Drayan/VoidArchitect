#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Entity {
    id: usize,
    generation: usize,
}

impl Entity {
    pub const NONE: Self = Self {
        id: usize::MAX,
        generation: usize::MAX,
    };

    pub(crate) fn new(id: usize, generation: usize) -> Self {
        Self { id, generation }
    }

    pub fn id(&self) -> usize {
        self.id
    }

    pub fn generation(&self) -> usize {
        self.generation
    }
}
