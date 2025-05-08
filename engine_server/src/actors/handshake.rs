use std::str::FromStr;

use actix::{
    Actor, ActorContext, ActorFutureExt, Addr, AsyncContext, Context, StreamHandler,
    dev::ContextFutureSpawner, fut,
};
use anyhow::Result;
use futures::{
    SinkExt, StreamExt,
    stream::{SplitSink, SplitStream},
};
use prost::{
    Message,
    bytes::{BufMut, BytesMut},
};
use rand::Rng;
use tokio::net::TcpStream;
use tokio_util::codec::{Decoder, Encoder, Framed};
use uuid::Uuid;
use void_architect_shared::messages::{
    ClientHello, ClientResponse, HandshakeFailure, HandshakeSuccess, NetworkPacket,
    ServerChallenge, network_packet,
};

use crate::actors::session_manager::RegisterSession;

use super::{messages::StreamTransfer, session_manager::SessionManagerActor};

const SECRET_KEY: [u8; 16] = [
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
    0x0F,
];

// --- Messages Codec ---
#[derive(Debug)]
enum HandshakeMessage {
    ClientHello(ClientHello),
    ServerChallenge(ServerChallenge), // Add other message types as needed
    ClientResponse(ClientResponse),
    HandshakeFailure(HandshakeFailure),
    HandshakeSuccess(HandshakeSuccess),
}

struct HandshakeCodec;

impl HandshakeCodec {
    fn new() -> Self {
        HandshakeCodec
    }

    fn identify_message_type(
        bytes: &mut BytesMut,
    ) -> Result<HandshakeMessage, std::io::Error> {
        // Check if the message is a ClientHello
        if bytes.len() < 4 {
            return Err(std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                "Not enough data to read length",
            ));
        }

        let packet = NetworkPacket::decode(bytes.as_ref())?;
        match packet.payload {
            Some(network_packet::Payload::ClientHello(client_hello)) => {
                Ok(HandshakeMessage::ClientHello(client_hello))
            }
            Some(network_packet::Payload::ClientResponse(client_response)) => {
                Ok(HandshakeMessage::ClientResponse(client_response))
            }
            Some(_) => Err(std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                "Unknown message type",
            )),
            None => Err(std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                "No payload found in packet",
            )),
        }
    }
}

impl Decoder for HandshakeCodec {
    type Item = HandshakeMessage;
    type Error = std::io::Error;

    fn decode(&mut self, src: &mut BytesMut) -> Result<Option<Self::Item>, Self::Error> {
        if src.is_empty() {
            return Ok(None);
        }

        // Try to identify the message type
        let msg_type = Self::identify_message_type(src)?;
        src.clear(); // Clear the buffer after decoding
        Ok(Some(msg_type))
    }
}

impl Encoder<HandshakeMessage> for HandshakeCodec {
    type Error = std::io::Error;

    fn encode(
        &mut self,
        msg: HandshakeMessage,
        dst: &mut BytesMut,
    ) -> Result<(), Self::Error> {
        let packet = match msg {
            HandshakeMessage::ServerChallenge(challenge) => NetworkPacket {
                payload: Some(network_packet::Payload::ServerChallenge(challenge)),
            },
            HandshakeMessage::HandshakeSuccess(success) => NetworkPacket {
                payload: Some(network_packet::Payload::HandshakeSuccess(success)),
            },
            HandshakeMessage::HandshakeFailure(failure) => NetworkPacket {
                payload: Some(network_packet::Payload::HandshakeFailure(failure)),
            },
            _ => {
                return Err(std::io::Error::new(
                    std::io::ErrorKind::InvalidData,
                    "Cannot encode this message type",
                ));
            }
        };

        let encoded = packet.encode_to_vec();
        dst.reserve(encoded.len());
        dst.put_slice(&encoded);

        Ok(())
    }
}

// --- Actor ---
enum HandshakeState {
    WaitingClientHello,
    SendingChallenge,
    WaitingClientResponse,
    Completed,
    Failed,
}

pub struct HandshakeActor {
    stream: std::net::TcpStream,
    framed_stream: Option<SplitStream<Framed<TcpStream, HandshakeCodec>>>,
    framed_sink: Option<SplitSink<Framed<TcpStream, HandshakeCodec>, HandshakeMessage>>,
    challenge: Vec<u8>,
    state: HandshakeState,
    client_uuid: Option<Uuid>,
    session_manager_addr: Addr<SessionManagerActor>,
}

impl HandshakeActor {
    /// Creates a new HandshakeActor.
    pub fn new(stream: TcpStream, addr: Addr<SessionManagerActor>) -> Result<Self> {
        let stream_std = stream.into_std()?;

        let std_stream_clone = stream_std.try_clone()?;

        let stream_for_framed = TcpStream::from_std(stream_std)?;

        let framed = Framed::new(stream_for_framed, HandshakeCodec::new());
        let (stream_sink, stream_read) = framed.split();

        Ok(HandshakeActor {
            stream: std_stream_clone,
            framed_stream: Some(stream_read),
            framed_sink: Some(stream_sink),
            challenge: Self::generate_challenge(),
            state: HandshakeState::WaitingClientHello,
            client_uuid: None,
            session_manager_addr: addr,
        })
    }

    fn generate_challenge() -> Vec<u8> {
        // Generate a random challenge nonce
        let mut rng = rand::rng();
        let mut challenge_bytes = [0u8; 16];
        rng.fill(&mut challenge_bytes);
        challenge_bytes.to_vec()
    }

    fn send_challenge(&mut self, ctx: &mut Context<Self>) {
        let challenge = ServerChallenge {
            challenge_nonce: self.challenge.clone(),
            protocol_version: "0.1.0".to_string(),
            server_version: "0.1.0".to_string(),
        };

        match self.send_message(HandshakeMessage::ServerChallenge(challenge), false, ctx) {
            Ok(_) => {
                log::info!("Challenge sent successfully");
            }
            Err(e) => {
                log::error!("Failed to send challenge: {}", e);
                ctx.stop();
            }
        }
    }

    fn calculate_challenge_response(
        challenge: &[u8],
        secret_key: &[u8],
    ) -> Result<Vec<u8>, String> {
        if challenge.len() != 16 {
            return Err("Challenge must be 16 bytes".to_string());
        }
        if secret_key.len() != 16 {
            return Err("Secret key must be 16 bytes".to_string());
        }
        let mut response = [0u8; 16];
        for i in 0..16 {
            response[i] = challenge[i] ^ secret_key[i];
        }
        Ok(response.to_vec())
    }

    fn verify_response(&self, response: ClientResponse) -> bool {
        // Verify the response using the SECRET_KEY
        let expected_response =
            match Self::calculate_challenge_response(&self.challenge, &SECRET_KEY) {
                Ok(resp) => resp,
                Err(e) => {
                    log::error!("Failed to calculate challenge response: {}", e);
                    return false;
                }
            };
        response.challenge_response == expected_response
    }

    fn send_message(
        &mut self,
        msg: HandshakeMessage,
        should_stop: bool,
        ctx: &mut Context<Self>,
    ) -> Result<(), String> {
        log::debug!("Sending message: {:?}", msg);
        if let Some(mut framed) = self.framed_sink.take() {
            let fut = async move {
                let result = framed.send(msg).await;
                (framed, result)
            };

            fut::wrap_future(fut)
                .map(move |(framed, result), act: &mut HandshakeActor, ctx| {
                    act.framed_sink = Some(framed);

                    match result {
                        Ok(_) => {
                            log::info!("Message sent successfully");
                            if should_stop {
                                ctx.stop();
                            }
                        }
                        Err(e) => {
                            log::error!("Failed to send message: {}", e)
                        }
                    }
                })
                .wait(ctx);
            Ok(())
        } else {
            log::error!("Failed to send message");
            ctx.stop();
            return Err("Failed to send message".to_string());
        }
    }

    fn send_failure(&mut self, reason: String, ctx: &mut Context<Self>) {
        let failure = HandshakeFailure { reason };
        match self.send_message(HandshakeMessage::HandshakeFailure(failure), true, ctx) {
            Ok(_) => {
                log::info!("Failure message sent successfully");
            }
            Err(e) => {
                log::error!("Failed to send failure message: {}", e);
                ctx.stop();
            }
        }
    }
}

impl Actor for HandshakeActor {
    type Context = Context<Self>;

    fn started(&mut self, ctx: &mut Self::Context) {
        log::info!("HandshakeActor started, waiting for ClientHello");

        if let Some(framed) = self.framed_stream.take() {
            ctx.add_stream(framed);
        } else {
            log::error!("Failed to create framed stream");
            ctx.stop();
        }
    }
}

// --- Handlers ---
impl StreamHandler<Result<HandshakeMessage, std::io::Error>> for HandshakeActor {
    fn handle(
        &mut self,
        msg: Result<HandshakeMessage, std::io::Error>,
        ctx: &mut Context<Self>,
    ) {
        log::trace!("Received message: {:?}", msg);
        match msg {
            Ok(msg) => match (&self.state, msg) {
                // Handle the ClientHello message
                (
                    HandshakeState::WaitingClientHello,
                    HandshakeMessage::ClientHello(client_hello),
                ) => {
                    log::debug!("Received ClientHello: {:?}", client_hello);
                    self.state = HandshakeState::SendingChallenge;
                    // Validate the UUID
                    match Uuid::from_str(client_hello.client_uuid.as_str()) {
                        Ok(uuid) => {
                            self.client_uuid = Some(uuid);
                        }
                        Err(e) => {
                            log::error!("Invalid UUID: {}", e);
                            self.state = HandshakeState::Failed;
                            self.send_failure("Invalid UUID".to_string(), ctx);
                            return;
                        }
                    }
                    // Send the challenge to the client
                    self.send_challenge(ctx);
                    self.state = HandshakeState::WaitingClientResponse;
                }
                // Handle the ClientResponse message
                (
                    HandshakeState::WaitingClientResponse,
                    HandshakeMessage::ClientResponse(client_response),
                ) => {
                    log::debug!("Received ClientResponse: {:?}", client_response);
                    // Verifiy the response from the client against the challenge
                    if self.verify_response(client_response) {
                        log::info!("Client response verified successfully");
                        self.state = HandshakeState::Completed;
                        // Send a RegisterSession message to the session manager
                        // with the client UUID and stream
                        if let Some(uuid) = self.client_uuid {
                            fut::wrap_future(self.session_manager_addr.send(
                                RegisterSession {
                                    handshake_addr: ctx.address(),
                                    client_uuid: uuid,
                                },
                            ))
                            .map(|result, actor: &mut HandshakeActor, ctx| match result {
                                Ok(res) => {
                                    log::info!("Session registered successfully: {:?}", res);
                                    // Send HandshakeSuccess message to the client
                                    let success = HandshakeSuccess {
                                        session_id: res.session_id,
                                        game_version: Some("0.1.0".to_string()),
                                        server_name: Some("Void Architect".to_string()),
                                    };
                                    match actor.send_message(
                                        HandshakeMessage::HandshakeSuccess(success),
                                        true,
                                        ctx,
                                    ) {
                                        Ok(_) => {
                                            log::info!("Handshake success message sent");
                                            //TODO: Transfer the stream to the session actor
                                            let stream_clone = match actor.stream.try_clone() {
                                                Ok(stream) => stream,
                                                Err(e) => {
                                                    log::error!(
                                                        "Failed to clone stream: {}",
                                                        e
                                                    );
                                                    actor.state = HandshakeState::Failed;
                                                    actor.send_failure(
                                                        "Failed to clone stream".to_string(),
                                                        ctx,
                                                    );
                                                    return;
                                                }
                                            };
                                            res.session_addr.do_send(StreamTransfer {
                                                stream: stream_clone,
                                            });
                                        }
                                        Err(e) => {
                                            log::error!(
                                                "Failed to send handshake success: {}",
                                                e
                                            );
                                            actor.state = HandshakeState::Failed;
                                            actor.send_failure(
                                                "Failed to send handshake success".to_string(),
                                                ctx,
                                            );
                                        }
                                    }
                                }
                                Err(e) => {
                                    log::error!("Failed to register session: {}", e);
                                    actor.state = HandshakeState::Failed;
                                    actor.send_failure(
                                        "Failed to register session".to_string(),
                                        ctx,
                                    );
                                }
                            })
                            .wait(ctx);
                        } else {
                            log::error!("Client UUID is not set");
                            self.state = HandshakeState::Failed;
                            self.send_failure("Client UUID is not set".to_string(), ctx);
                        }
                    } else {
                        log::error!("Client response verification failed");
                        self.state = HandshakeState::Failed;
                        self.send_failure(
                            "Client response verification failed".to_string(),
                            ctx,
                        );
                    }
                }
                // If we got to this point, we received a message that we didn't expect
                _ => {
                    log::warn!("Unexpected message");
                    self.state = HandshakeState::Failed;
                    self.send_failure("Unexpected message".to_string(), ctx);
                }
            },
            // Handle the error case
            Err(e) => {
                log::error!("Error while processing message: {}", e);
                self.state = HandshakeState::Failed;
                self.send_failure("Error while processing message".to_string(), ctx);
            }
        }
    }
}
