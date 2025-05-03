use prost::Message;
use tokio::{
    io::{AsyncReadExt, AsyncWriteExt},
    net::TcpListener,
};
use void_architect_shared::messages::{
    ClientHello, ClientResponse, HandshakeFailure, HandshakeSucess, ServerChallenge,
};

use crate::config::get_config;

const MAX_MESSAGE_SIZE: usize = 1024;
const SECRET_KEY: [u8; 16] = [
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
    0x0F,
];

pub struct NetworkSystem {
    server: Option<TcpListener>,
}

impl NetworkSystem {
    pub fn new() -> Result<Self, String> {
        log::info!("Initializing network system...");

        Ok(NetworkSystem { server: None })
    }

    pub async fn start(&mut self) -> Result<(), String> {
        let (host, port) = get_config();
        // Create a TCP listener
        self.server = match TcpListener::bind(format!("{host}:{port}")).await {
            Ok(listener) => Some(listener),
            Err(e) => {
                log::error!("Failed to bind to {host}:{port} : {e:#?}");
                return Err("Failed to bind to server".to_string());
            }
        };
        log::info!("Server is listening on {host}:{port}");
        Ok(())
    }

    pub async fn accept(&self) -> Result<(), String> {
        if let Some(listener) = &self.server {
            match listener.accept().await {
                Ok((stream, addr)) => {
                    log::info!("Accepted connection from {addr:#?}");
                    // Handle the connection in a separate task
                    //NOTE: Maybe we should handle the connection directly in the SessionSystem
                    tokio::spawn(NetworkSystem::handle_connection(stream));
                }
                Err(e) => {
                    log::error!("Failed to accept connection: {e:#?}");
                    return Err("Failed to accept connection".to_string());
                }
            }
        } else {
            log::error!("Server is not started");
            return Err("Server is not started".to_string());
        }
        Ok(())
    }

    async fn handle_connection(mut stream: tokio::net::TcpStream) {
        // Handle the connection here
        // For now, we'll just log the address of the connected client
        log::info!("Handling connection from {:#?}", stream.peer_addr());

        //TODO: Create the session and give the stream to the session.
        NetworkSystem::handle_handshake(&mut stream).await;
    }

    async fn handle_handshake(stream: &mut tokio::net::TcpStream) -> Result<(), String> {
        // 1. The handshake protocol state that the client need to send a ClientHello, the
        // server wait for this message.
        let client_hello = NetworkSystem::read_message::<ClientHello>(stream).await?;
        log::info!("Received: {client_hello:#?}");

        // 2. The server should check is the client uuid is allowed to connect to the server.

        // 3. If the client is allowed to connect, the server should generate a challenge and send it
        // to the client.
        let mut challenge = [0u8; 16];
        rand::fill(&mut challenge);
        log::debug!("Generated challenge: {challenge:?}");
        let challenge_server = ServerChallenge {
            challenge_nonce: challenge.to_vec(),
            server_version: "0.1.0".to_string(),
            protocol_version: "1".to_string(),
        };
        NetworkSystem::write_message(stream, &challenge_server)
            .await
            .map_err(|e| format!("Failed to write to stream: {e:#?}"))?;
        log::debug!("Sent: {challenge_server:#?}");

        // 4. The client should respond with a ClientResponse message containing the challenge
        // response.
        let challenge_response =
            NetworkSystem::calculate_challenge_response(&challenge, &SECRET_KEY)?;
        let client_response = NetworkSystem::read_message::<ClientResponse>(stream).await?;
        log::debug!("Received: {client_response:#?}");
        if client_response.challenge_response != challenge_response {
            log::error!("Invalid challenge response");
            // The client response is invalid, we should close the connection.
            let handshade_failure = HandshakeFailure {
                reason: "Invalid challenge response".to_string(),
            };
            NetworkSystem::write_message(stream, &handshade_failure)
                .await
                .map_err(|e| format!("Failed to write to stream: {e:#?}"))?;
            stream
                .shutdown()
                .await
                .map_err(|e| format!("Failed to shutdown stream: {e:#?}"))?;
            return Err("Invalid challenge response".to_string());
        }

        // 5. If the client response is valid, the server should create a session and send a success message to the
        // client.
        let handshake_success = HandshakeSucess {
            session_id: 1, //TODO: Create a session and get the session id
            server_name: Some("Void Architect".to_string()),
            game_version: Some("0.1.0".to_string()),
        };
        NetworkSystem::write_message(stream, &handshake_success)
            .await
            .map_err(|e| format!("Failed to write to stream: {e:#?}"))?;
        log::debug!("Sent: {handshake_success:#?}");
        Ok(())
    }

    async fn read_message<T: Message + Default>(
        stream: &mut tokio::net::TcpStream,
    ) -> Result<T, String> {
        stream.readable().await.map_err(|e| format!("Failed to read from stream: {e:#?}"))?;

        let mut buffer = [0; MAX_MESSAGE_SIZE];
        let n = stream
            .read(&mut buffer)
            .await
            .map_err(|e| format!("Failed to read from stream: {e:#?}"))?;
        if n == 0 {
            return Err("Client disconnected".to_string());
        }
        log::info!("Received {n} bytes from client");

        // Decode the message
        let message = T::decode(&buffer[..n]).map_err(|e| e.to_string())?;
        Ok(message)
    }

    async fn write_message<T: Message>(
        stream: &mut tokio::net::TcpStream,
        message: &T,
    ) -> Result<(), String> {
        let mut buffer = Vec::new();
        message.encode(&mut buffer).map_err(|e| e.to_string())?;
        stream
            .write_all(&buffer)
            .await
            .map_err(|e| format!("Failed to write to stream: {e:#?}"))?;
        Ok(())
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
}
