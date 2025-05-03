use std::sync::Arc;

use network::NetworkSystem;
use persistence::PersistenceSystem;
use session::SessionsSystem;
use tokio::sync::RwLock;

mod config;
mod network;
mod persistence;
mod session;

pub struct EngineServer {
    persistence: Arc<PersistenceSystem>,
    sessions: Arc<SessionsSystem>,
    network: Arc<RwLock<NetworkSystem>>,
}

impl EngineServer {
    pub fn new() -> Result<Self, String> {
        env_logger::Builder::new().filter_level(log::LevelFilter::max()).init();

        log::info!("Initializing engine server...");
        Ok(EngineServer {
            persistence: Arc::new(PersistenceSystem::new()?),
            sessions: Arc::new(SessionsSystem::new()?),
            network: Arc::new(RwLock::new(NetworkSystem::new()?)),
        })
    }

    #[tokio::main]
    pub async fn run(&self) -> Result<(), String> {
        self.network.write().await.start().await?;

        log::info!("Engine server is running...");
        loop {
            // Here you would typically handle incoming connections, process requests, etc.
            // For now, we'll just sleep to simulate work being done.
            self.network.read().await.accept().await?;
            // Accept a new connection
            // let (stream, _) = listener.accept().await.unwrap();
            // log::info!("Accepted connection from {:#?}", stream.peer_addr());

            // Swap a new task to handle the connection
            // tokio::spawn(handle_connection(stream));
        }
    }
}

impl Drop for EngineServer {
    fn drop(&mut self) {
        log::info!("Shutting down engine server...");
    }
}
