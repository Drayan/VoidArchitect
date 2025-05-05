//! Actor responsible for handling the client-server handshake protocol.

use actix::{Actor, ActorContext, Addr, Context, Handler, Message}; // Added ActorContext
use tokio::net::TcpStream;
use uuid::Uuid;

use crate::actors::session_manager::SessionManagerActor; // Assuming SessionManagerActor exists

/// Represents the possible states during the handshake process.
#[derive(Debug, Clone, PartialEq, Eq)]
enum HandshakeState {
    /// Waiting for the client to send ClientHello.
    WaitingClientHello,
    /// Waiting for the client to send ClientResponse after ServerChallenge.
    WaitingClientResponse,
    /// Handshake completed successfully.
    Completed,
    /// Handshake failed.
    Failed,
}

/// Actor responsible for managing the handshake process for a single connection.
///
/// It follows the protocol defined in `DOCS.md`:
/// 1. Receives `ClientHello` (with client UUID).
/// 2. Checks UUID against an allow list (via PersistenceActor - TODO).
/// 3. Sends `ServerChallenge`.
/// 4. Receives `ClientResponse`.
/// 5. Verifies response.
/// 6. If successful, notifies `SessionManagerActor` to create a `SessionActor`.
/// 7. If failed at any point, terminates.
#[derive(Debug)]
pub struct HandshakeActor {
    /// The TCP stream associated with the client connection.
    /// This will likely be wrapped in a codec later.
    stream: Option<TcpStream>, // Option because it will be taken by SessionActor on success
    /// Address of the SessionManagerActor to notify on success.
    session_manager: Addr<SessionManagerActor>,
    /// The unique identifier provided by the client.
    client_uuid: Option<Uuid>,
    /// The random nonce sent in the ServerChallenge.
    challenge_nonce: Option<Vec<u8>>, // Placeholder type
    /// The current state of the handshake process.
    state: HandshakeState,
    // TODO: Add Addr<PersistenceActor> for UUID check
}

impl HandshakeActor {
    /// Creates a new HandshakeActor.
    pub fn new(
        stream: TcpStream,
        session_manager: Addr<SessionManagerActor>,
        // persistence_actor: Addr<PersistenceActor>, // TODO: Add persistence actor
    ) -> Self {
        HandshakeActor {
            stream: Some(stream),
            session_manager,
            client_uuid: None,
            challenge_nonce: None,
            state: HandshakeState::WaitingClientHello,
            // persistence_actor, // TODO
        }
    }

    /// Placeholder for handling ClientHello reception.
    fn handle_client_hello(&mut self, _ctx: &mut Context<Self>, _uuid: Uuid) {
        // TODO:
        // 1. Request UUID check from PersistenceActor.
        // 2. On success:
        //    - Store client_uuid.
        //    - Generate nonce.
        //    - Store nonce.
        //    - Send ServerChallenge over stream.
        //    - Change state to WaitingClientResponse.
        // 3. On failure (UUID not allowed):
        //    - Send Disconnect message over stream.
        //    - Change state to Failed.
        //    - Stop the actor.
        log::info!("HandshakeActor: Received ClientHello (placeholder)");
        self.state = HandshakeState::WaitingClientResponse; // Placeholder transition
    }

    /// Placeholder for handling ClientResponse reception.
    fn handle_client_response(&mut self, ctx: &mut Context<Self>, _response: Vec<u8>) {
        // TODO:
        // 1. Verify response against stored nonce.
        // 2. On success:
        //    - Generate server_session_id.
        //    - Send HandshakeSuccess over stream.
        //    - Notify SessionManagerActor (send stream, client_uuid, session_id).
        //    - Change state to Completed.
        //    - Stop the actor (SessionActor takes over).
        // 3. On failure:
        //    - Send Disconnect message over stream.
        //    - Change state to Failed.
        //    - Stop the actor.
        log::info!("HandshakeActor: Received ClientResponse (placeholder)");

        // Placeholder: Assume success for skeleton
        if let Some(stream) = self.stream.take() {
            let client_uuid = self.client_uuid.unwrap_or_else(Uuid::new_v4); // Placeholder
            // # Reason: Using rand::random for a placeholder u32 session ID.
            // This needs proper generation logic later.
            let session_id: u32 = rand::random(); // Placeholder server-generated session ID

            // Notify Session Manager
            self.session_manager.do_send(HandshakeResult::Success {
                stream,
                client_uuid,
                session_id,
            });

            self.state = HandshakeState::Completed;
            log::info!("HandshakeActor: Handshake successful (placeholder), stopping.");
            ctx.stop();
        } else {
            log::error!("HandshakeActor: Stream already taken, cannot complete handshake.");
            self.state = HandshakeState::Failed;
            ctx.stop();
        }
    }

    /// Placeholder for handling network read events.
    fn handle_network_read(&mut self, ctx: &mut Context<Self>, data: Vec<u8>) {
        log::debug!("HandshakeActor: Received data ({} bytes)", data.len());
        // TODO: Deserialize message based on current state
        match self.state {
            HandshakeState::WaitingClientHello => {
                // Placeholder: Assume it's a valid ClientHello containing a UUID
                let uuid = Uuid::new_v4(); // Placeholder
                self.handle_client_hello(ctx, uuid);
            }
            HandshakeState::WaitingClientResponse => {
                // Placeholder: Assume it's a valid ClientResponse
                self.handle_client_response(ctx, data);
            }
            _ => {
                log::warn!(
                    "HandshakeActor: Received data in unexpected state: {:?}",
                    self.state
                );
            }
        }
    }

    /// Placeholder for handling network errors or disconnection.
    fn handle_disconnect(&mut self, ctx: &mut Context<Self>) {
        log::info!("HandshakeActor: Client disconnected during handshake.");
        self.state = HandshakeState::Failed;
        ctx.stop();
    }
}

impl Actor for HandshakeActor {
    type Context = Context<Self>;

    fn started(&mut self, ctx: &mut Context<Self>) {
        log::info!("HandshakeActor started.");
        // TODO: Start reading from the stream.
        // This requires integrating with Actix's stream handling,
        // possibly using actix-codec or manual async read loops within the actor.
        // For the skeleton, we'll simulate receiving messages via handlers.

        // Example of simulating receiving ClientHello after a short delay
        // In a real scenario, this would be driven by actual network reads.
        // ctx.run_later(std::time::Duration::from_secs(1), |act, ctx| {
        //     act.handle_network_read(ctx, vec![]); // Simulate read
        // });
    }

    fn stopping(&mut self, _ctx: &mut Context<Self>) -> actix::Running {
        log::info!("HandshakeActor stopping. Final state: {:?}", self.state);
        actix::Running::Stop
    }
}

// --- Messages ---

/// Message to initiate the handshake process for this actor.
/// Sent by the NetworkListenerActor (or equivalent).
#[derive(Message)]
#[rtype(result = "()")]
pub struct StartHandshake; // May not be needed if actor starts immediately

impl Handler<StartHandshake> for HandshakeActor {
    type Result = ();

    fn handle(&mut self, _msg: StartHandshake, _ctx: &mut Context<Self>) -> Self::Result {
        log::info!("HandshakeActor: Explicit StartHandshake received (may be redundant).");
        // Actor already starts reading in `started` or via stream handling setup.
    }
}

/// Message sent by the HandshakeActor to the SessionManagerActor upon successful handshake.
#[derive(Message, Debug)]
#[rtype(result = "()")]
pub enum HandshakeResult {
    Success {
        stream: TcpStream,
        client_uuid: Uuid,
        session_id: u32, // Server-assigned session ID
    },
    Failure {
        // Could include reason
    },
}

// --- Basic Tests ---
#[cfg(test)]
mod tests {
    use super::*;
    use crate::actors::session_manager::SessionManagerActor;
    use actix::Actor;
    use tokio::net::TcpListener; // Import the actual actor

    // Note: MockSessionManager removed as the creation test can use the real one.
    // Mocks might be needed for more complex interaction tests later.

    #[actix::test] // Use actix::test for consistency
    async fn test_handshake_actor_creation() {
        // Bind to a local port to get a TcpStream
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let addr = listener.local_addr().unwrap();
        let stream = TcpStream::connect(addr).await.unwrap();
        let _other_stream = listener.accept().await.unwrap().0; // Accept the connection

        let session_manager_addr = SessionManagerActor::new().start(); // Use the actual actor
        let actor = HandshakeActor::new(stream, session_manager_addr.clone());

        assert_eq!(actor.state, HandshakeState::WaitingClientHello);
        // Basic creation test, doesn't test the full flow yet.
    }
}
