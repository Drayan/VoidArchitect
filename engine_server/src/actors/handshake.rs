use actix::{
    Actor, ActorContext, ActorFutureExt, AsyncContext, Context, StreamHandler,
    dev::ContextFutureSpawner, fut,
};
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
use void_architect_shared::messages::{
    ClientHello, ClientResponse, HandshakeFailure, HandshakeSuccess, ServerChallenge,
};

const SECRET_KEY: [u8; 16] = [
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
    0x0F,
];

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
                "Message too short",
            ));
        }

        if let Ok(decode) = ClientHello::decode(bytes.as_ref()) {
            return Ok(HandshakeMessage::ClientHello(decode));
        } else if let Ok(decode) = ClientResponse::decode(bytes.as_ref()) {
            return Ok(HandshakeMessage::ClientResponse(decode));
        } else {
            return Err(std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                "Unknown message type",
            ));
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
        let encoded = match msg {
            HandshakeMessage::ServerChallenge(challenge) => challenge.encode_to_vec(),
            HandshakeMessage::HandshakeSuccess(success) => success.encode_to_vec(),
            HandshakeMessage::HandshakeFailure(failure) => failure.encode_to_vec(),
            _ => {
                return Err(std::io::Error::new(
                    std::io::ErrorKind::InvalidData,
                    "Cannot encode this message type",
                ));
            }
        };

        dst.reserve(encoded.len());
        dst.put_slice(&encoded);

        Ok(())
    }
}

enum HandshakeState {
    WaitingClientHello,
    SendingChallenge,
    WaitingClientResponse,
    Completed,
    Failed,
}

pub struct HandshakeActor {
    framed_stream: Option<SplitStream<Framed<TcpStream, HandshakeCodec>>>,
    framed_sink: Option<SplitSink<Framed<TcpStream, HandshakeCodec>, HandshakeMessage>>,
    challenge: Vec<u8>,
    state: HandshakeState,
}

impl HandshakeActor {
    /// Creates a new HandshakeActor.
    pub fn new(stream: TcpStream) -> Self {
        let framed = Framed::new(stream, HandshakeCodec::new());
        let (stream_sink, stream_read) = framed.split();
        HandshakeActor {
            framed_stream: Some(stream_read),
            framed_sink: Some(stream_sink),
            challenge: Self::generate_challenge(),
            state: HandshakeState::WaitingClientHello,
        }
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

impl StreamHandler<Result<HandshakeMessage, std::io::Error>> for HandshakeActor {
    fn handle(
        &mut self,
        msg: Result<HandshakeMessage, std::io::Error>,
        ctx: &mut Context<Self>,
    ) {
        log::trace!("Received message: {:?}", msg);
        match msg {
            Ok(msg) => match (&self.state, msg) {
                (
                    HandshakeState::WaitingClientHello,
                    HandshakeMessage::ClientHello(client_hello),
                ) => {
                    log::debug!("Received ClientHello: {:?}", client_hello);
                    self.state = HandshakeState::SendingChallenge;
                    self.send_challenge(ctx);
                    self.state = HandshakeState::WaitingClientResponse;
                }
                (
                    HandshakeState::WaitingClientResponse,
                    HandshakeMessage::ClientResponse(client_response),
                ) => {
                    log::debug!("Received ClientResponse: {:?}", client_response);
                    if self.verify_response(client_response) {
                        log::info!("Client response verified successfully");
                        self.state = HandshakeState::Completed;
                        //TODO: Handoff to SessionManagerActor
                    } else {
                        log::error!("Client response verification failed");
                        self.state = HandshakeState::Failed;
                        self.send_message(
                            HandshakeMessage::HandshakeFailure(HandshakeFailure {
                                reason: "Invalid response".to_string(),
                            }),
                            true,
                            ctx,
                        )
                        .unwrap_or_else(|e| {
                            log::error!("Failed to send handshake failure: {}", e);
                        });
                    }
                }
                _ => {
                    log::warn!("Unexpected message");
                    self.state = HandshakeState::Failed;
                    self.send_message(
                        HandshakeMessage::HandshakeFailure(HandshakeFailure {
                            reason: "Unexpected message".to_string(),
                        }),
                        true,
                        ctx,
                    )
                    .unwrap_or_else(|e| {
                        log::error!("Failed to send handshake failure: {}", e);
                    });
                }
            },
            Err(e) => {
                log::error!("Error while processing message: {}", e);
                self.state = HandshakeState::Failed;
                self.send_message(
                    HandshakeMessage::HandshakeFailure(HandshakeFailure {
                        reason: "Error processing message".to_string(),
                    }),
                    true,
                    ctx,
                )
                .unwrap_or_else(|e| {
                    log::error!("Failed to send handshake failure: {}", e);
                });
            }
        }
    }
}
