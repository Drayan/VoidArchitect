use actix::{Actor, Addr, Context, Handler, Message, Recipient};
use std::collections::HashMap;
use tokio::net::TcpStream;
use uuid::Uuid;

// use crate::actors::session::SessionActor; // TODO: Uncomment when SessionActor is implemented

// --- Messages ---

/// Message sent to the SessionManagerActor to register a new session.
#[derive(Message)]
#[rtype(result = "()")]
pub struct RegisterSession {
    /// The ID of the session to register.
    pub session_id: u32,
    /// The stream associated with the session.
    pub stream: TcpStream,
    /// The UUID of the client.
    pub client_uuid: Uuid,
}

/// Message sent by SessionActor when it stops or disconnects.
#[derive(Message)]
#[rtype(result = "()")]
pub struct UnregisterSession {
    /// The ID of the session to unregister.
    pub session_id: u32, // Use u32 to match HandshakeResult
}

// --- Actor ---

/// Manages active client sessions.
///
/// Receives `HandshakeResult` messages, spawns `SessionActor` instances on success,
/// and tracks their lifecycle.
#[derive(Debug)]
pub struct SessionManagerActor {
    /// Map of active sessions: session_id (u32) -> Address of SessionActor.
    // sessions: HashMap<u32, Addr<SessionActor>>, // TODO: Uncomment when SessionActor is implemented
    /// Counter for assigning unique u32 session IDs.
    next_session_id: u32,
}

// Implement Default manually since HashMap is commented out
impl Default for SessionManagerActor {
    fn default() -> Self {
        Self::new()
    }
}

impl SessionManagerActor {
    /// Creates a new SessionManagerActor.
    pub fn new() -> Self {
        SessionManagerActor {
            // sessions: HashMap::new(), // TODO: Uncomment when SessionActor is implemented
            next_session_id: 1, // Start IDs from 1
        }
    }

    /// Generates the next available session ID.
    /// Handles potential overflow, although unlikely with u32.
    fn get_next_session_id(&mut self) -> u32 {
        let id = self.next_session_id;
        // Increment and wrap around on overflow. Check if the wrapped ID is already in use.
        // This is a simple approach; a more robust system might reuse IDs or use u64.
        self.next_session_id = self.next_session_id.wrapping_add(1);
        if self.next_session_id == 0 {
            // Avoid session ID 0 if desired
            self.next_session_id = 1;
        }
        // TODO: Uncomment collision check when sessions map is active
        // while self.sessions.contains_key(&self.next_session_id) {
        //      self.next_session_id = self.next_session_id.wrapping_add(1);
        //      if self.next_session_id == 0 {
        //         self.next_session_id = 1;
        //     }
        // }
        // For now, just wrap without checking collision
        while self.next_session_id == 0 {
            // Ensure we don't assign 0 if it wraps directly to 0
            self.next_session_id = self.next_session_id.wrapping_add(1);
        }
        // Removed extra closing brace here
        id
    }
}

impl Actor for SessionManagerActor {
    type Context = Context<Self>;

    fn started(&mut self, _ctx: &mut Self::Context) {
        log::info!("SessionManagerActor started.");
    }

    fn stopped(&mut self, _ctx: &mut Self::Context) {
        log::info!("SessionManagerActor stopped.");
        // Potential cleanup if needed, though SessionActors should unregister themselves.
    }
}

// --- Handlers ---

impl Handler<RegisterSession> for SessionManagerActor {
    type Result = ();

    fn handle(&mut self, msg: RegisterSession, ctx: &mut Self::Context) -> Self::Result {
        match msg {
            RegisterSession {
                stream,
                client_uuid,
                session_id: _,
            } => {
                // Ignore session_id from handshake for now, assign our own
                let new_session_id = self.get_next_session_id();

                log::info!(
                    "Handshake successful for client {}. Assigning session ID: {}",
                    client_uuid,
                    new_session_id
                );

                // TODO: Uncomment collision check when sessions map is active
                // if self.sessions.contains_key(&new_session_id) {
                //     log::error!(
                //         "CRITICAL: Session ID collision detected for ID: {}. This should not happen.",
                //         new_session_id
                //     );
                //     // Decide how to handle this critical error (e.g., disconnect new client)
                //     return;
                // }

                // TODO: Start the SessionActor when implemented.
                // let session_addr = SessionActor::new(
                //     new_session_id,
                //     stream,
                //     client_uuid,
                //     ctx.address().recipient(), // Pass SessionManager's recipient for unregistering
                //     /* other required addresses like UniverseActor, PersistenceActor */
                //     new_session_id,
                //     stream,
                //     client_uuid,
                //     ctx.address().recipient(), // Pass SessionManager's recipient for unregistering
                //     /* other required addresses like UniverseActor, PersistenceActor */
                // ).start();

                // Placeholder: Log that we would start the actor.
                log::info!(
                    "Placeholder: Would start SessionActor for session {}",
                    new_session_id
                );
                // let placeholder_addr = ctx.address().recipient::<UnregisterSession>().clone(); // Dummy recipient not needed now

                // TODO: Insert the actual Addr<SessionActor> when implemented.
                // self.sessions.insert(new_session_id, session_addr);

                // log::info!("Total active sessions: {}", self.sessions.len()); // Can't report count yet
            }
        }
    }
}

impl Handler<UnregisterSession> for SessionManagerActor {
    type Result = ();

    fn handle(&mut self, msg: UnregisterSession, _ctx: &mut Self::Context) -> Self::Result {
        log::info!("Received request to unregister session: {}", msg.session_id);

        // TODO: Uncomment when sessions map is active
        // if self.sessions.remove(&msg.session_id).is_some() {
        //     log::info!(
        //         "Session {} removed successfully.",
        //         msg.session_id
        //     );
        // } else {
        //     log::warn!(
        //         "Attempted to unregister non-existent session ID: {}",
        //         msg.session_id
        //     );
        // }
        // log::info!("Total active sessions: {}", self.sessions.len());
        log::info!(
            "Placeholder: Would remove session {} from map.",
            msg.session_id
        );
    }
}

// --- Tests ---

#[cfg(test)]
mod tests {
    use super::*;
    use actix::Actor;
    use tokio::net::TcpListener; // Required for stream creation in tests

    #[actix::test]
    async fn test_session_manager_actor_creation() {
        // Simple test to ensure the actor can be created.
        let manager = SessionManagerActor::new();
        // assert!(manager.sessions.is_empty()); // TODO: Uncomment when sessions map is active
        assert_eq!(manager.next_session_id, 1);

        // Start the actor to check basic lifecycle methods (started/stopped logs).
        let addr = manager.start();
        assert!(addr.connected());
        // Actor stops automatically when addr is dropped or system stops.
    }

    #[actix::test]
    async fn test_session_id_generation() {
        let mut manager = SessionManagerActor::new();
        assert_eq!(manager.get_next_session_id(), 1);
        assert_eq!(manager.get_next_session_id(), 2);
        assert_eq!(manager.next_session_id, 3); // Check internal state
    }

    #[actix::test]
    async fn test_session_id_wrapping() {
        let mut manager = SessionManagerActor::new();
        manager.next_session_id = u32::MAX;
        assert_eq!(manager.get_next_session_id(), u32::MAX);
        assert_eq!(manager.get_next_session_id(), 1); // Should wrap to 1 (skipping 0)
        assert_eq!(manager.get_next_session_id(), 2);
    }

    // TODO: Add tests for HandshakeResult handling
    // once SessionActor is implemented and mocking/test setup is available.
    // Need to simulate sending HandshakeResult::Success and HandshakeResult::Failure.
}
