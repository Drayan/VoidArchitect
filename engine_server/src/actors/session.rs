use std::time::Duration;

use actix::dev::ContextFutureSpawner;
use actix::{Actor, ActorContext, ActorFutureExt, Addr, AsyncContext, Context, Handler, fut};
use anyhow::{Result, bail};
use futures::stream::{SplitSink, SplitStream};
use futures::{SinkExt, StreamExt};
use prost::Message;
use prost::bytes::{BufMut, BytesMut};
use tokio::{net::TcpStream, time::Instant};
use tokio_util::codec::{Decoder, Encoder, Framed};
use uuid::Uuid;
use void_architect_shared::messages::object_transform::{Attribute, MessageType};
use void_architect_shared::messages::{
    ObjectTransform, Quaternion, SessionHeartbeat, Vector3,
};

use super::messages::StreamTransfer;
use super::session_manager::{SessionManagerActor, UnregisterSession};
use super::universe::{
    SubscribeToUniverse, UniverseActor, UniverseUpdate, UnsubscribeFromUniverse,
};

// --- Codec ---
#[derive(Debug)]
enum SessionMessage {
    SessionHeartbeat(SessionHeartbeat),
    ObjectTransform(ObjectTransform),
}

struct SessionCodec;

impl SessionCodec {
    fn new() -> Self {
        SessionCodec
    }
}

// The Decoder trait will be used to decode incoming messages from the stream.
// But for now, the client will not send anything, so we can leave it unimplemented.
impl Decoder for SessionCodec {
    type Item = SessionMessage;
    type Error = std::io::Error;

    fn decode(&mut self, src: &mut BytesMut) -> Result<Option<SessionMessage>, Self::Error> {
        if src.is_empty() {
            return Ok(None);
        }

        Ok(None)
    }
}

impl Encoder<SessionMessage> for SessionCodec {
    type Error = std::io::Error;

    fn encode(&mut self, msg: SessionMessage, dst: &mut BytesMut) -> Result<(), Self::Error> {
        // let encoded = match msg {
        //     SessionMessage::SessionHeartbeat(heartbeat) => heartbeat.encode_to_vec(),
        //     SessionMessage::ObjectTransform(transform) => transform.encode_to_vec(),
        //     _ => {
        //         return Err(std::io::Error::new(
        //             std::io::ErrorKind::InvalidInput,
        //             "Unsupported message type",
        //         ));
        //     }
        // };
        let packet = match msg {
            // SessionMessage::SessionHeartbeat(heartbeat) => {
            //     void_architect_shared::messages::NetworkPacket {
            //         payload: Some(void_architect_shared::messages::network_packet::Payload::SessionHeartbeat(heartbeat)),
            //     }
            // }
            SessionMessage::ObjectTransform(transform) => {
                void_architect_shared::messages::NetworkPacket {
                    payload: Some(void_architect_shared::messages::network_packet::Payload::ObjectTransform(transform)),
                }
            },
            _ => {
                return Err(std::io::Error::new(
                    std::io::ErrorKind::InvalidInput,
                    "Unsupported message type",
                ));
            }
        };

        let encoded = packet.encode_to_vec();
        // log::trace!("Encoded message: {:?}", encoded);
        dst.reserve(encoded.len());
        dst.put_slice(&encoded);

        Ok(())
    }
}

// --- Actor ---
pub struct SessionActor {
    /// The ID of the session.
    id: u32,
    /// The stream associated with the session.
    stream: Option<std::net::TcpStream>,
    framed_stream: Option<SplitStream<Framed<TcpStream, SessionCodec>>>,
    framed_sink: Option<SplitSink<Framed<TcpStream, SessionCodec>, SessionMessage>>,

    /// The UUID of the client.
    client_uuid: Uuid,
    /// The address of the session manager.
    manager_addr: Addr<SessionManagerActor>,
    /// The address of the universe actor.
    universe_addr: Addr<UniverseActor>,
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
        universe_addr: Addr<UniverseActor>,
    ) -> Self {
        SessionActor {
            id,
            stream: None,
            framed_sink: None,
            framed_stream: None,
            client_uuid,
            manager_addr,
            universe_addr,
            last_heartbeat: Instant::now(),
            heartbeat_interval: Duration::from_secs(30), // Set heartbeat interval to 30 seconds
        }
    }

    fn send_message(
        &mut self,
        msg: SessionMessage,
        should_stop: bool,
        ctx: &mut Context<Self>,
    ) -> Result<()> {
        // log::trace!("Sending message: {:?}", msg);
        if let Some(mut framed) = self.framed_sink.take() {
            let fut = async move {
                let result = framed.send(msg).await;
                (framed, result)
            };

            fut::wrap_future(fut)
                .map(move |(framed, result), act: &mut SessionActor, ctx| {
                    act.framed_sink = Some(framed);

                    match result {
                        Ok(_) => {
                            if should_stop {
                                ctx.stop();
                            }
                        }
                        Err(e) => {
                            log::error!("Failed to send message: {}", e);
                            //TODO: Should probably propagate the error upwards.
                            ctx.stop();
                        }
                    }
                })
                .wait(ctx);
            Ok(())
        } else {
            log::error!("Failed to send message");
            ctx.stop();
            bail!("Failed to send message");
        }
    }
}

impl Actor for SessionActor {
    type Context = actix::Context<Self>;

    fn started(&mut self, ctx: &mut Self::Context) {
        // Initialize the actor when it starts
        log::info!("SessionActor started with ID: {}", self.id);

        self.universe_addr.do_send(SubscribeToUniverse {
            session_addr: ctx.address(),
        });
    }

    fn stopped(&mut self, ctx: &mut Self::Context) {
        // Cleanup when the actor stops
        log::info!("SessionActor stopped with ID: {}", self.id);

        self.universe_addr.do_send(UnsubscribeFromUniverse {
            session_addr: ctx.address(),
        });

        self.manager_addr.do_send(UnregisterSession {
            session_id: self.id,
            client_uuid: self.client_uuid,
        });
    }
}

// --- Handlers ---
impl Handler<StreamTransfer> for SessionActor {
    type Result = ();

    fn handle(&mut self, msg: StreamTransfer, ctx: &mut Self::Context) -> Self::Result {
        // Handle stream transfer
        log::trace!("Received stream transfer.");

        // Set the stream and create a framed stream
        let stream_clone = match msg.stream.try_clone() {
            Ok(stream) => stream,
            Err(e) => {
                log::error!("Failed to clone stream: {}", e);
                ctx.stop();
                return;
            }
        };
        self.stream = Some(stream_clone);

        let stream_for_framed = match TcpStream::from_std(msg.stream) {
            Ok(stream) => stream,
            Err(e) => {
                log::error!("Failed to convert stream to tokio stream: {}", e);
                ctx.stop();
                return;
            }
        };
        let framed = Framed::new(stream_for_framed, SessionCodec::new());
        let (sink, stream) = framed.split();
        self.framed_sink = Some(sink);
        self.framed_stream = Some(stream);
    }
}

impl Handler<UniverseUpdate> for SessionActor {
    type Result = ();

    fn handle(&mut self, msg: UniverseUpdate, ctx: &mut Self::Context) -> Self::Result {
        // Handle universe updates
        // log::trace!("Received universe update: {:?}", msg);
        //TODO: Send the update to the client
        let transform = ObjectTransform {
            object_session_id: 0, //TODO: Implement session -> Object mapping
            attributes: vec![
                Attribute::Position as i32,
                Attribute::Rotation as i32,
                Attribute::Scale as i32,
            ],
            r#type: MessageType::ObjectTransformUpdate as i32,
            position: Some(Vector3 {
                x: msg.position.x,
                y: msg.position.y,
                z: msg.position.z,
            }),
            rotation: Some(Quaternion {
                x: msg.rotation.i,
                y: msg.rotation.j,
                z: msg.rotation.k,
                w: msg.rotation.w,
            }),
            scale: Some(Vector3 {
                x: msg.scale.x,
                y: msg.scale.y,
                z: msg.scale.z,
            }),
        };

        // Send the message to the client
        match self.send_message(SessionMessage::ObjectTransform(transform), false, ctx) {
            Ok(_) => {}
            Err(e) => {
                log::error!("Failed to send universe update: {}", e);
                ctx.stop();
            }
        }
    }
}
