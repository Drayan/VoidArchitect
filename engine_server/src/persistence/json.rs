use super::backend::PersistenceBackend;

pub struct JSONPersistenceBackend {
    file_path: String,
}

impl JSONPersistenceBackend {
    pub fn new(file_path: &str) -> Self {
        JSONPersistenceBackend {
            file_path: file_path.to_string(),
        }
    }
}

impl PersistenceBackend for JSONPersistenceBackend {
    fn save(&self, data: &str) -> Result<(), String> {
        Ok(())
    }

    fn load(&self) -> Result<String, String> {
        Ok("".to_string())
    }
}
