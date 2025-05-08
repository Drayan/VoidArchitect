use std::net::SocketAddr;

use actix::{Actor, ActorContext, Addr, AsyncContext, Context, fut::future};
use tokio::net::TcpListener;

use crate::actors::handshake;

use super::session_manager::SessionManagerActor;

pub struct NetworkListenerActor {
    addr: SocketAddr,
    session_manager_addr: Addr<SessionManagerActor>,
}

impl NetworkListenerActor {
    /// Creates a new NetworkListenerActor.
    pub fn new(addr: SocketAddr, session_manager_addr: Addr<SessionManagerActor>) -> Self {
        NetworkListenerActor {
            addr,
            session_manager_addr,
        }
    }
}

impl Actor for NetworkListenerActor {
    type Context = Context<Self>;

    fn started(&mut self, ctx: &mut Self::Context) {
        log::info!(
            "NetworkListenerActor started, attempting to bind to {}",
            self.addr
        );

        let listener = match std::net::TcpListener::bind(self.addr) {
            Ok(listener) => listener,
            Err(e) => {
                log::error!("Failed to bind TCP listener to {}: {}", self.addr, e);
                ctx.stop();
                return;
            }
        };

        // Make the listener non-blocking for Tokio integration.
        if let Err(e) = listener.set_nonblocking(true) {
            log::error!("Failed to set listener to non-blocking: {}", e);
            ctx.stop();
            return;
        }

        let listener = match TcpListener::from_std(listener) {
            Ok(listener) => listener,
            Err(e) => {
                log::error!("Failed to convert std listener to tokio listener: {}", e);
                ctx.stop();
                return;
            }
        };
        log::info!("Successfully bound listener to {}", self.addr);

        let session_manager_addr = self.session_manager_addr.clone();
        let fut = async move {
            loop {
                match listener.accept().await {
                    Ok((stream, addr)) => {
                        log::info!("Accepted connection from {}", addr);
                        let handshake_actor = match handshake::HandshakeActor::new(
                            stream,
                            session_manager_addr.clone(),
                        ) {
                            Ok(actor) => actor,
                            Err(e) => {
                                log::error!("Failed to create HandshakeActor: {}", e);
                                continue;
                            }
                        };
                        handshake_actor.start();
                    }
                    Err(e) => {
                        log::error!("Failed to accept connection: {}", e);
                        break;
                    }
                }
            }
        };

        ctx.spawn(future::wrap_future(fut));
    }
}
