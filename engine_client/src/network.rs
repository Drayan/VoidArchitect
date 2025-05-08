use futures::{SinkExt, StreamExt};
use tokio::net::TcpStream;
use tokio_util::codec::{Decoder, Encoder, Framed};

use anyhow::{Result, bail};
use prost::{
    Message,
    bytes::{BufMut, BytesMut},
};
use void_architect_shared::messages::{
    ClientHello, ClientResponse, HandshakeFailure, HandshakeSuccess, NetworkPacket,
    ObjectTransform, ServerChallenge, network_packet,
};

const SECRET_KEY: [u8; 16] = [
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
    0x0F,
];

#[derive(Debug)]
pub(super) enum MessageType {
    ClientHello(ClientHello),
    ServerChallenge(ServerChallenge),
    ClientResponse(ClientResponse),
    HandshakeFailure(HandshakeFailure),
    HandshakeSuccess(HandshakeSuccess),
    ObjectTransform(ObjectTransform),
}
struct ClientCodec;

impl ClientCodec {
    fn new() -> Self {
        ClientCodec
    }

    fn identify_message_type(src: &mut BytesMut) -> Result<MessageType, std::io::Error> {
        if src.len() < 4 {
            return Err(std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                "Not enough data to read length",
            ));
        }

        let packet = NetworkPacket::decode(src.as_ref())?;
        match packet.payload {
            Some(network_packet::Payload::ServerChallenge(challenge)) => {
                Ok(MessageType::ServerChallenge(challenge))
            }
            Some(network_packet::Payload::HandshakeFailure(failure)) => {
                Ok(MessageType::HandshakeFailure(failure))
            }
            Some(network_packet::Payload::HandshakeSuccess(success)) => {
                Ok(MessageType::HandshakeSuccess(success))
            }
            Some(network_packet::Payload::ObjectTransform(transform)) => {
                Ok(MessageType::ObjectTransform(transform))
            }
            Some(_) => {
                log::error!("Unknown message type");
                Err(std::io::Error::new(
                    std::io::ErrorKind::InvalidData,
                    "Unknown message type",
                ))
            }
            None => {
                log::error!("No payload in message");
                Err(std::io::Error::new(
                    std::io::ErrorKind::InvalidData,
                    "No payload in message",
                ))
            }
        }
    }
}

impl Decoder for ClientCodec {
    type Item = MessageType;
    type Error = std::io::Error;

    fn decode(&mut self, src: &mut BytesMut) -> Result<Option<Self::Item>, Self::Error> {
        if src.is_empty() {
            return Ok(None);
        }

        let msg = Self::identify_message_type(src)?;
        // log::trace!("Decoded message: {:?}", msg);
        src.clear();
        Ok(Some(msg))
    }
}

impl Encoder<MessageType> for ClientCodec {
    type Error = std::io::Error;

    fn encode(&mut self, item: MessageType, dst: &mut BytesMut) -> Result<(), Self::Error> {
        let packet = match item {
            MessageType::ClientHello(msg) => NetworkPacket {
                payload: Some(network_packet::Payload::ClientHello(msg)),
            },
            MessageType::ClientResponse(msg) => NetworkPacket {
                payload: Some(network_packet::Payload::ClientResponse(msg)),
            },
            _ => {
                return Err(std::io::Error::new(
                    std::io::ErrorKind::InvalidData,
                    "Cannot encode this message type",
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

pub(super) struct NetworkSystem {
    sender: std::sync::mpsc::Sender<MessageType>,
    framed_stream: Option<Framed<TcpStream, ClientCodec>>,
}

impl NetworkSystem {
    pub fn new(sender: std::sync::mpsc::Sender<MessageType>) -> Self {
        NetworkSystem {
            sender,
            framed_stream: None,
        }
    }

    pub async fn initialize(&mut self) -> Result<()> {
        // Initialize network resources here

        // Open a TCP connection
        let stream = TcpStream::connect("127.0.0.1:4242").await?;
        log::info!("Connected to server");

        let framed = Framed::new(stream, ClientCodec::new());
        self.framed_stream = Some(framed);

        self.perform_handshake().await?;
        Ok(())
    }

    pub async fn run(
        &mut self,
        mut shutdown_signal: tokio::sync::oneshot::Receiver<()>,
    ) -> Result<()> {
        // Main loop for the network system
        loop {
            // Check for shutdown signal
            if let Ok(()) = shutdown_signal.try_recv() {
                log::info!("Shutdown signal received");
                break;
            }

            // Check for incoming messages
            match self.receive_message().await {
                Ok(msg) => {
                    // Process the message
                    self.sender.send(msg)?
                }
                Err(e) => {
                    log::error!("Failed to decode message: {}", e);
                    bail!("Failed to decode message");
                }
            }
        }

        Ok(())
    }

    pub async fn shutdown(&mut self) -> Result<()> {
        // Clean up network resources here
        match self.framed_stream {
            Some(_) => {
                drop(self.framed_stream.take());
            }
            _ => (),
        }
        log::info!("Network system shut down");
        Ok(())
    }

    async fn send_message(&mut self, msg: MessageType) -> Result<()> {
        // Send data over the network
        if let Some(stream) = &mut self.framed_stream {
            stream.send(msg).await?;
        } else {
            log::error!("No active stream to send data");
            bail!("No active stream to send data");
        }
        Ok(())
    }

    async fn receive_message(&mut self) -> Result<MessageType> {
        // Receive data from the network
        if let Some(stream) = &mut self.framed_stream {
            if let Some(msg) = stream.next().await {
                match msg {
                    Ok(msg) => {
                        return Ok(msg);
                    }
                    Err(e) => {
                        log::error!("Failed to decode message: {}", e);
                        bail!("Failed to decode message");
                    }
                }
            } else {
                log::error!("Failed to receive message");
                bail!("Failed to receive message");
            }
        } else {
            log::error!("No active stream to receive data");
            bail!("No active stream to receive data");
        }
    }

    fn calculate_challenge_response(challenge: &[u8], secret_key: &[u8]) -> Result<Vec<u8>> {
        if challenge.len() != 16 {
            bail!("Challenge must be 16 bytes");
        }
        if secret_key.len() != 16 {
            bail!("Secret key must be 16 bytes");
        }
        let mut response = [0u8; 16];
        for i in 0..16 {
            response[i] = challenge[i] ^ secret_key[i];
        }
        Ok(response.to_vec())
    }

    async fn perform_handshake(&mut self) -> Result<()> {
        // Perform the handshake with the server
        // This is where you would send the ClientHello message and receive the ServerChallenge

        // 1. Send ClientHello message
        let client_hello = ClientHello {
            client_version: "0.1.0".to_string(),
            client_uuid: uuid::Uuid::new_v4().to_string(),
            protocol_version: "1".to_string(),
        };

        log::debug!("Sent ClientHello: {:?}", client_hello);
        self.send_message(MessageType::ClientHello(client_hello)).await?;

        // 2. Wait for ServerChallenge message
        let response = match self.receive_message().await? {
            MessageType::ServerChallenge(challenge) => challenge,
            _ => {
                log::error!("Expected ServerChallenge, but received different message");
                return Err(anyhow::anyhow!("Expected ServerChallenge"));
            }
        };
        log::debug!("Received ServerChallenge: {:?}", response);

        // 3. Calculate the challenge response
        let challenge = response.challenge_nonce;
        let response = match Self::calculate_challenge_response(&challenge, &SECRET_KEY) {
            Ok(response) => response,
            Err(e) => {
                log::error!("Failed to calculate challenge response: {}", e);
                return Err(anyhow::anyhow!("Failed to calculate challenge response"));
            }
        };
        // log::debug!("Challenge response: {:?}", response);

        // 4. Send ClientResponse message
        let client_response = ClientResponse {
            challenge_response: response,
        };
        log::debug!("Sent ClientResponse: {:?}", client_response);
        self.send_message(MessageType::ClientResponse(client_response)).await?;

        // 5. Wait for ServerResponse message
        match self.receive_message().await? {
            MessageType::HandshakeSuccess(success) => {
                log::debug!("Received HandshakeSuccess: {:?}", success);
            }
            MessageType::HandshakeFailure(failure) => {
                //TODO: Handle handshake failure and report to the user
                log::error!("Handshake failed: {:?}", failure);
                bail!("Handshake failed: {:?}", failure);
            }
            _ => {
                log::error!(
                    "Expected HandshakeSuccess or HandshakeFailure, but received different message"
                );
                bail!("Expected HandshakeSuccess or HandshakeFailure");
            }
        };

        // At some point, the client should send a ClientReady message ?
        // It will probably be sent at another point in the code, when the client is ready to start
        // the game. But for now, we can just send it here.
        // TODO: Send ClientReady message

        Ok(())
    }
}
