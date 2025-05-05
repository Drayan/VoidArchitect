use actix::Actor;
use actors::network_listener::NetworkListenerActor;

mod actors;
mod config;

pub struct EngineServer {}

impl EngineServer {
    pub fn new() -> Result<Self, String> {
        env_logger::Builder::new().filter_level(log::LevelFilter::max()).init();

        log::info!("Initializing engine server...");
        Ok(EngineServer {})
    }

    pub fn run(&self) -> Result<(), String> {
        let actor_system = actix::System::new();
        actor_system.block_on(Self::run_loop());

        actor_system.run().map_err(|e| {
            log::error!("Failed to run actor system: {e:#?}");
            format!("Actor system error: {e:#?}")
        })?;
        Ok(())
    }

    async fn run_loop() {
        //TODO: Retrieve the address from the config
        let network_listener =
            NetworkListenerActor::new(std::net::SocketAddr::from(([127, 0, 0, 1], 4242)));
        network_listener.start();
        log::info!("Engine server is running...");
    }
}

impl Drop for EngineServer {
    fn drop(&mut self) {
        log::info!("Shutting down engine server...");
    }
}
