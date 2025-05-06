use std::time::Duration;

use actix::{Actor, Addr};
use tokio::{net::TcpStream, time::Instant};
use uuid::Uuid;

use super::session_manager::SessionManagerActor;

// --- Actor ---
pub struct SessionActor {
    /// The ID of the session.
    id: u32,
    /// The stream associated with the session.
    stream: Option<TcpStream>,
    /// The UUID of the client.
    client_uuid: Uuid,
    /// The address of the session manager.
    manager_addr: Addr<SessionManagerActor>,
    /// The address of the universe actor.
    // universe_addr: Addr<UniverseActor>, //TODO: Uncomment when UniverseActor is implemented
    /// The last time a heartbeat was received.
    last_heartbeat: Instant,
    /// The interval for sending heartbeat messages.
    heartbeat_interval: Duration,
}

impl SessionActor {
    /// Creates a new SessionActor.
    pub fn new(
        id: u32,
        client_uuid: Uuid,
        manager_addr: Addr<SessionManagerActor>,
        // universe_addr: Addr<UniverseActor>, //TODO: Uncomment when UniverseActor is implemented
    ) -> Self {
        SessionActor {
            id,
            stream: None,
            client_uuid,
            manager_addr,
            // universe_addr: universe_addr, //TODO: Uncomment when UniverseActor is implemented
            last_heartbeat: Instant::now(),
            heartbeat_interval: Duration::from_secs(30), // Set heartbeat interval to 30 seconds
        }
    }
}

impl Actor for SessionActor {
    type Context = actix::Context<Self>;

    fn started(&mut self, ctx: &mut Self::Context) {
        // Initialize the actor when it starts
        log::info!("SessionActor started with ID: {}", self.id);
    }

    fn stopped(&mut self, ctx: &mut Self::Context) {
        // Cleanup when the actor stops
        log::info!("SessionActor stopped with ID: {}", self.id);
    }
}
