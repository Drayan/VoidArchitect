mod actors;
mod config;

pub struct EngineServer {}

impl EngineServer {
    pub fn new() -> Result<Self, String> {
        env_logger::Builder::new().filter_level(log::LevelFilter::max()).init();

        log::info!("Initializing engine server...");
        Ok(EngineServer {})
    }

    #[tokio::main]
    pub async fn run(&self) -> Result<(), String> {
        Ok(())
    }
}

impl Drop for EngineServer {
    fn drop(&mut self) {
        log::info!("Shutting down engine server...");
    }
}
