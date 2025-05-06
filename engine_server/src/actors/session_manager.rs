use std::collections::HashMap;

use actix::{Actor, Addr, AsyncContext, Handler, Message, MessageResult};
use uuid::Uuid;

use super::{handshake::HandshakeActor, session::SessionActor};

// --- Actor ---
pub struct SessionManagerActor {
    /// Map of active sessions: session_id (u32) -> Address of SessionActor.
    sessions: HashMap<u32, Addr<SessionActor>>,
    users: HashMap<Uuid, u32>, // Map of client UUIDs to session IDs
}

impl SessionManagerActor {
    /// Creates a new SessionManagerActor.
    pub fn new() -> Self {
        SessionManagerActor {
            sessions: HashMap::new(),
            users: HashMap::new(),
        }
    }

    fn get_next_session_id(&mut self) -> u32 {
        if self.sessions.len() as u32 == u32::MAX {
            // If all IDs are used, we can't have more sessions
            log::error!("All session IDs are used");
            panic!("All session IDs are used"); //TODO: Handle this case more gracefully
            // But it's unlikely to happen in practice since u32 can hold a lot of values
            // It's way more likely that the server will run out of memory or other resources
        }

        let mut proposed_id = self.sessions.len() as u32;
        proposed_id = proposed_id.wrapping_add(1); // Start from the next ID

        while self.sessions.contains_key(&proposed_id) {
            proposed_id = proposed_id.wrapping_add(1); // Increment until we find an unused ID, wrapping around if necessary
        }
        proposed_id
    }

    fn is_user_registered(&self, client_uuid: &Uuid) -> bool {
        self.users.contains_key(client_uuid)
    }
}

impl Actor for SessionManagerActor {
    type Context = actix::Context<Self>;

    fn started(&mut self, ctx: &mut Self::Context) {
        // Initialize the actor when it starts
        log::info!("SessionManagerActor started");
    }

    fn stopped(&mut self, ctx: &mut Self::Context) {
        // Cleanup when the actor stops
        log::info!("SessionManagerActor stopped");
    }
}

// --- Messages ---
/// Message sent to the SessionManagerActor to register a new session.
#[derive(Message)]
#[rtype(result = "SessionRegistrationSuccess")]
pub struct RegisterSession {
    pub handshake_addr: Addr<HandshakeActor>,
    pub client_uuid: Uuid,
}

/// Result of successful session registration.
#[derive(Debug)]
pub struct SessionRegistrationSuccess {
    pub session_id: u32,
    pub session_addr: Addr<SessionActor>,
}

/// Message sent by SessionActor when it stops or disconnects.
#[derive(Message)]
#[rtype(result = "()")]
pub struct UnregisterSession {
    session_id: u32,
    client_uuid: Uuid,
}

// --- Handlers ---
impl Handler<RegisterSession> for SessionManagerActor {
    type Result = MessageResult<RegisterSession>;

    fn handle(&mut self, msg: RegisterSession, ctx: &mut Self::Context) -> Self::Result {
        // Handle session registration
        log::info!("Registering session for client UUID: {}", msg.client_uuid);

        if self.is_user_registered(&msg.client_uuid) {
            log::error!("Client UUID {} is already registered", msg.client_uuid);
            //TODO: Send failure message to HandshakeActor
        }

        // Generate a new session ID
        let session_id = self.get_next_session_id();
        let session = SessionActor::new(session_id, msg.client_uuid, ctx.address());
        // Start the session actor
        let session_addr = session.start();
        // Store the session address in the sessions map
        self.sessions.insert(session_id, session_addr.clone());
        self.users.insert(msg.client_uuid, session_id);

        MessageResult(SessionRegistrationSuccess {
            session_id,
            session_addr,
        })
    }
}

impl Handler<UnregisterSession> for SessionManagerActor {
    type Result = ();

    fn handle(&mut self, msg: UnregisterSession, ctx: &mut Self::Context) -> Self::Result {
        // Handle session unregistration
        log::info!(
            "Unregistering session with ID {}, for client UUID: {}",
            msg.session_id,
            msg.client_uuid
        );

        //TODO: Implement session unregistration logic
    }
}
