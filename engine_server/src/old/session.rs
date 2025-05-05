// engine_server/src/actors/session.rs
use actix::{
    Actor, ActorContext, ActorFutureExt, Addr, AsyncContext, Context, Handler, Message,
    ResponseActFuture, Running, StreamHandler, System, WrapFuture, fut,
    io::{FramedWrite, WriteHandler},
};
use log::{debug, error, info, warn};
use std::time::{Duration, Instant};
use tokio::net::TcpStream;
use tokio_util::codec::{FramedRead, LinesCodec}; // Placeholder Codec
use uuid::Uuid;

use super::{
    session_manager::{SessionManagerActor, UnregisterSession},
    universe::UniverseActor,
};

// --- Messages ---

/// Message sent from SessionManager to start the session actor.
/// Contains the stream, session ID, and manager address.
#[derive(Message)]
#[rtype(result = "()")]
pub struct StartSession {
    pub id: u32,
    pub stream: FramedRead<TcpStream, LinesCodec>, // Placeholder, replace with actual codec
    pub manager_addr: Addr<SessionManagerActor>,
    pub universe_addr: Addr<UniverseActor>,
}

/// Message sent from UniverseActor with state updates.
#[derive(Message, Clone)] // Clone needed if UniverseActor sends to multiple sessions
#[rtype(result = "()")]
pub struct CubeStateUpdate {
    // Placeholder for actual state data (e.g., position, rotation)
    pub data: String,
}

/// Message sent by SessionActor to UniverseActor to subscribe to updates.
#[derive(Message)]
#[rtype(result = "()")] // Or perhaps Result<(), SubscribeError>
pub struct SubscribeToCubeUpdates;

/// Internal message to stop the actor.
#[derive(Message)]
#[rtype(result = "()")]
struct Stop;

// --- Actor Definition ---

/// Actor responsible for managing a single client connection (session).
///
/// Handles receiving messages from the client, sending messages to the client,
/// interacting with the UniverseActor for game state, and managing its own lifecycle.
pub struct SessionActor {
    /// Unique identifier for this session.
    id: u32,
    /// Framed network connection to the client.
    /// We need WriteHalf for sending messages.
    // writer: FramedWrite<tokio::io::WriteHalf<TcpStream>, LinesCodec>, // Placeholder
    /// Address of the SessionManagerActor for lifecycle management (e.g., unregistering).
    manager_addr: Addr<SessionManagerActor>,
    /// Address of the UniverseActor to interact with game state.
    universe_addr: Addr<UniverseActor>,
    /// Timestamp of the last heartbeat received from the client.
    last_heartbeat: Instant,
    /// Interval for checking heartbeats.
    heartbeat_interval: Duration,
}

impl SessionActor {
    /// Creates a new SessionActor.
    /// The stream is typically passed via a message after creation or during startup.
    pub fn new(
        id: u32,
        manager_addr: Addr<SessionManagerActor>,
        universe_addr: Addr<UniverseActor>,
    ) -> Self {
        SessionActor {
            id,
            // writer: // Initialized in started or via message
            manager_addr,
            universe_addr,
            last_heartbeat: Instant::now(),
            heartbeat_interval: Duration::from_secs(10), // Example interval
        }
    }

    /// Starts the heartbeat checking mechanism.
    fn start_heartbeat(&self, ctx: &mut Context<Self>) {
        ctx.run_interval(self.heartbeat_interval, |act, ctx| {
            if Instant::now().duration_since(act.last_heartbeat) > act.heartbeat_interval * 2 {
                warn!(
                    "Session {}: Client heartbeat timeout. Disconnecting.",
                    act.id
                );
                // Notify SessionManager before stopping
                act.manager_addr.do_send(UnregisterSession { session_id: act.id }); // Corrected field name
                ctx.stop();
            }
            // TODO: Optionally send a ping message to the client here
        });
    }
}

impl Actor for SessionActor {
    type Context = Context<Self>;

    /// Called when the actor is first started.
    ///
    /// Sets up the heartbeat check and subscribes to universe updates.
    fn started(&mut self, ctx: &mut Context<Self>) {
        info!("Session actor {} started.", self.id);
        self.start_heartbeat(ctx);

        // Subscribe to updates from the UniverseActor
        let subscribe_future = self.universe_addr.send(SubscribeToCubeUpdates);

        // Convert the MailboxError into a more general error if needed, or just log it.
        let actor_future = subscribe_future.into_actor(self).map(|res, _act, ctx| {
            match res {
                Ok(_) => debug!(
                    "Session {}: Successfully subscribed to Universe updates.",
                    _act.id
                ),
                Err(e) => {
                    error!(
                        "Session {}: Failed to subscribe to Universe updates: {}. Stopping.",
                        _act.id, e
                    );
                    // Notify SessionManager before stopping
                    _act.manager_addr.do_send(UnregisterSession {
                        session_id: _act.id,
                    });
                    ctx.stop();
                }
            }
        });

        ctx.wait(actor_future);
    }

    /// Called when the actor is stopping.
    ///
    /// Notifies the SessionManagerActor that this session is ending.
    fn stopping(&mut self, _ctx: &mut Context<Self>) -> Running {
        info!("Session actor {} stopping.", self.id);
        // Important: Notify the manager that this session is gone.
        // This might be redundant if the manager initiated the stop, but good practice.
        self.manager_addr.do_send(UnregisterSession {
            session_id: self.id,
        });
        Running::Stop
    }
}

// --- Message Handlers ---

/// Handler for receiving messages from the client stream.
impl StreamHandler<Result<String, std::io::Error>> for SessionActor {
    fn handle(&mut self, item: Result<String, std::io::Error>, ctx: &mut Context<Self>) {
        match item {
            Ok(msg_str) => {
                debug!("Session {}: Received message: {}", self.id, msg_str);
                self.last_heartbeat = Instant::now();
                // TODO: Parse the message (e.g., from JSON or Protobuf)
                // TODO: Handle different message types (e.g., input, requests)
                // TODO: Potentially forward messages to UniverseActor or other actors
                if msg_str.trim() == "PING" {
                    // Example: Respond to a simple PING
                    // self.writer.write("PONG".to_string());
                    debug!("Session {}: Responded to PING", self.id);
                } else if msg_str.trim() == "STOP" {
                    // Example: Client requests disconnection
                    info!("Session {}: Client requested stop.", self.id);
                    self.manager_addr.do_send(UnregisterSession {
                        session_id: self.id,
                    });
                    ctx.stop();
                }
            }
            Err(e) => {
                if e.kind() == std::io::ErrorKind::UnexpectedEof {
                    info!("Session {}: Client disconnected.", self.id);
                } else {
                    error!("Session {}: Error reading from stream: {}", self.id, e);
                }
                self.manager_addr.do_send(UnregisterSession {
                    session_id: self.id,
                });
                ctx.stop(); // Stop actor on stream error/disconnection
            }
        }
    }

    fn finished(&mut self, ctx: &mut Context<Self>) {
        info!("Session {}: Client stream finished.", self.id);
        self.manager_addr.do_send(UnregisterSession {
            session_id: self.id,
        });
        ctx.stop();
    }
}

/// Handler for state updates received from the UniverseActor.
impl Handler<CubeStateUpdate> for SessionActor {
    type Result = ();

    fn handle(&mut self, msg: CubeStateUpdate, _ctx: &mut Context<Self>) {
        debug!(
            "Session {}: Received CubeStateUpdate: {}",
            self.id, msg.data
        );
        // TODO: Convert CubeStateUpdate data to the network format (e.g., Protobuf ObjectUpdate)
        // TODO: Serialize the network message
        // TODO: Send the serialized message to the client via self.writer
        // Example placeholder:
        // let network_msg = format!("UPDATE: {}", msg.data);
        // self.writer.write(network_msg);
    }
}

/// Handler for the internal Stop message.
impl Handler<Stop> for SessionActor {
    type Result = ();

    fn handle(&mut self, _msg: Stop, ctx: &mut Context<Self>) {
        info!("Session {}: Received internal stop request.", self.id);
        self.manager_addr.do_send(UnregisterSession {
            session_id: self.id,
        });
        ctx.stop();
    }
}

/// Handler for Write errors (if using FramedWrite).
impl WriteHandler<std::io::Error> for SessionActor {
    fn error(&mut self, err: std::io::Error, ctx: &mut Context<Self>) -> Running {
        error!("Session {}: Error writing to client: {}", self.id, err);
        self.manager_addr.do_send(UnregisterSession {
            session_id: self.id,
        });
        Running::Stop // Stop actor if we can't write to the client
    }
}

// --- Unit Tests ---

#[cfg(test)]
mod tests {
    use super::*;
    use actix::Actor;
    use tokio::sync::mpsc;

    // Mock Actor for SessionManager
    struct MockSessionManager {
        unregister_tx: mpsc::Sender<u32>,
    }
    impl Actor for MockSessionManager {
        type Context = Context<Self>;
    }
    impl Handler<UnregisterSession> for MockSessionManager {
        type Result = ();
        fn handle(&mut self, msg: UnregisterSession, _ctx: &mut Context<Self>) {
            self.unregister_tx.try_send(msg.session_id).unwrap();
        }
    }

    // Mock Actor for Universe
    struct MockUniverse {
        subscribe_tx: mpsc::Sender<Addr<SessionActor>>,
    }
    impl Actor for MockUniverse {
        type Context = Context<Self>;
    }
    impl Handler<SubscribeToCubeUpdates> for MockUniverse {
        type Result = (); // Assuming success for mock
        fn handle(&mut self, _msg: SubscribeToCubeUpdates, ctx: &mut Context<Self>) {
            // In a real scenario, we'd need the SessionActor's address.
            // For this basic test, we just confirm the message is received.
            // A more complex test could pass the SessionActor addr via the message.
            // self.subscribe_tx.try_send(ctx.address()).unwrap(); // Can't easily get SessionActor addr here
            debug!("MockUniverse received SubscribeToCubeUpdates");
        }
    }

    // #[actix::test]
    // async fn test_session_actor_creation_and_start() {
    //     let (unregister_tx, mut unregister_rx) = mpsc::channel(1);
    //     let (subscribe_tx, _subscribe_rx) = mpsc::channel(1); // Channel for subscribe message

    //     let session_id = rand::random::<u32>();

    //     // Start mock actors
    //     let manager_addr = MockSessionManager { unregister_tx }.start();
    //     let universe_addr = MockUniverse { subscribe_tx }.start();

    //     // Create and start SessionActor
    //     let session_addr =
    //         SessionActor::new(session_id, manager_addr.clone(), universe_addr.clone()).start();

    //     // Give the actor time to start and send subscribe message
    //     tokio::time::sleep(Duration::from_millis(50)).await;

    //     // Stop the actor internally to test stopping logic
    //     session_addr.do_send(Stop);

    //     // Check if UnregisterSession was sent to the mock manager
    //     match tokio::time::timeout(Duration::from_secs(1), unregister_rx.recv()).await {
    //         Ok(Some(id)) => assert_eq!(
    //             id, session_id,
    //             "SessionManager did not receive correct UnregisterSession message"
    //         ),
    //         Ok(None) => panic!("Unregister channel closed unexpectedly"),
    //         Err(_) => panic!("Timed out waiting for UnregisterSession message"),
    //     }

    //     // We can't easily check if SubscribeToCubeUpdates was *sent* without more complex mocking,
    //     // but we know the actor started and stopped correctly.
    // }
    // #[actix::test]
    // async fn test_session_actor_heartbeat_timeout() {
    //     let (unregister_tx, mut unregister_rx) = mpsc::channel(1);
    //     let (subscribe_tx, _subscribe_rx) = mpsc::channel(1);

    //     let session_id = Uuid::new_v4();
    //     let manager_addr = MockSessionManager { unregister_tx }.start();
    //     let universe_addr = MockUniverse { subscribe_tx }.start();

    //     // Create SessionActor with a very short heartbeat interval for testing
    //     let mut actor =
    //         SessionActor::new(session_id, manager_addr.clone(), universe_addr.clone());
    //     actor.heartbeat_interval = Duration::from_millis(50); // Short interval

    //     let addr = actor.start();

    //     // Wait longer than the timeout period (interval * 2)
    //     tokio::time::sleep(Duration::from_millis(150)).await;

    //     // Check if the actor stopped itself and sent UnregisterSession
    //     match tokio::time::timeout(Duration::from_secs(1), unregister_rx.recv()).await {
    //         Ok(Some(id)) => assert_eq!(
    //             id, session_id,
    //             "SessionManager did not receive correct UnregisterSession on heartbeat timeout"
    //         ),
    //         Ok(None) => panic!("Unregister channel closed unexpectedly on heartbeat timeout"),
    //         Err(_) => {
    //             panic!("Timed out waiting for UnregisterSession message on heartbeat timeout")
    //         }
    //     }

    //     // Verify the actor is stopped
    //     assert!(
    //         !addr.connected(),
    //         "Actor should be stopped after heartbeat timeout"
    //     );
    // }
}
