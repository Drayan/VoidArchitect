pub trait PersistenceBackend {
    fn save(&self, data: &str) -> Result<(), String>;
    fn load(&self) -> Result<String, String>;
}
