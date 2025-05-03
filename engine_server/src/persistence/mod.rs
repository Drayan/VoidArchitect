pub mod backend;
pub mod json;

pub struct PersistenceSystem {
    backend: Box<dyn backend::PersistenceBackend>,
}

impl PersistenceSystem {
    pub fn new() -> Result<Self, String> {
        let backend = json::JSONPersistenceBackend::new("data.json");
        Ok(PersistenceSystem {
            backend: Box::new(backend),
        })
    }
}
