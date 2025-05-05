use actix::{fut::wrap_future, prelude::*};
use log::{error, info};
use std::net::SocketAddr;
use tokio::net::TcpListener;

/// Actor responsible for listening for incoming TCP connections.
///
/// Binds to a specified address and port. Upon accepting a new connection,
/// it will eventually hand it off to a `HandshakeActor` (implementation pending).
#[derive(Debug)]
pub struct NetworkListenerActor {
    addr: SocketAddr,
    // TODO: Add Addr<SessionManagerActor> or similar for handoff later
}

impl NetworkListenerActor {
    /// Creates a new NetworkListenerActor.
    pub fn new(addr: SocketAddr) -> Self {
        NetworkListenerActor { addr }
    }
}

impl Actor for NetworkListenerActor {
    type Context = Context<Self>;

    /// Called when the actor is started.
    ///
    /// Binds the TCP listener and starts the connection acceptance loop.
    fn started(&mut self, ctx: &mut Self::Context) {
        info!(
            "NetworkListenerActor started, attempting to bind to {}",
            self.addr
        );

        // Use block_on here because binding is synchronous within the actor's start phase.
        // The actual accept loop will be asynchronous.
        let listener = match std::net::TcpListener::bind(self.addr) {
            Ok(listener) => listener,
            Err(e) => {
                error!("Failed to bind TCP listener to {}: {}", self.addr, e);
                // Stop the actor if binding fails.
                ctx.stop();
                return;
            }
        };

        // Make the listener non-blocking for Tokio integration.
        if let Err(e) = listener.set_nonblocking(true) {
            error!("Failed to set listener to non-blocking: {}", e);
            ctx.stop();
            return;
        }

        // Convert std::net::TcpListener to tokio::net::TcpListener
        let listener = match TcpListener::from_std(listener) {
            Ok(listener) => listener,
            Err(e) => {
                error!("Failed to convert std listener to tokio listener: {}", e);
                ctx.stop();
                return;
            }
        };

        info!("Successfully bound listener to {}", self.addr);

        // Spawn the accept loop as a future within the actor's context.
        ctx.spawn(
            wrap_future::<_, Self>(async move {
                loop {
                    match listener.accept().await {
                        Ok((stream, peer_addr)) => {
                            info!("Accepted new connection from: {}", peer_addr);
                            // TODO: Story 1.7 - Handoff 'stream' to a new HandshakeActor.
                            // For now, just drop the stream to close the connection immediately
                            // after accepting, as we don't have the next step implemented.
                            drop(stream);
                        }
                        Err(e) => {
                            error!("Failed to accept connection: {}", e);
                            // Decide if the error is fatal. Some errors might be temporary.
                            // For now, we continue listening. Consider adding logic to stop
                            // on specific errors if needed.
                        }
                    }
                }
            })
            .map(|_, _, _| {}), // Map result to () as required by spawn
        );
    }

    fn stopping(&mut self, _ctx: &mut Self::Context) -> Running {
        info!("NetworkListenerActor stopping.");
        Running::Stop
    }
}

// Basic messages (can be expanded later)
#[derive(Message)]
#[rtype(result = "()")]
struct StopListener;

impl Handler<StopListener> for NetworkListenerActor {
    type Result = ();

    fn handle(&mut self, _msg: StopListener, ctx: &mut Context<Self>) {
        info!("Received StopListener message.");
        ctx.stop();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::net::{Ipv4Addr, SocketAddrV4};

    // Note: Testing the actual network binding and accepting in unit tests is complex.
    // This test primarily checks instantiation. Integration tests are better suited
    // for verifying the network behavior.

    #[actix::test]
    async fn test_network_listener_actor_creation() {
        let addr = SocketAddr::V4(SocketAddrV4::new(Ipv4Addr::LOCALHOST, 0)); // Use port 0 for OS assignment
        let actor = NetworkListenerActor::new(addr);
        assert_eq!(actor.addr, addr);

        // We can't easily test `started` here without a real Actix system
        // and potentially interfering with other tests or system ports.
        // We'll rely on integration tests for the full behavior.
    }
}
