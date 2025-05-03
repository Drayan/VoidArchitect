//! Main entry point for the VoidArchitect client application.
//!
//! This binary connects to the server, exchanges initial handshake messages using Protobuf,
//! and launches the engine client window. It demonstrates basic networking and engine startup.

use prost::Message;
use tokio::{
    io::{AsyncReadExt, AsyncWriteExt},
    net::TcpStream,
};
use void_architect_engine_client::EngineClient;
use void_architect_shared::messages::{ClientHello, ClientResponse, ServerChallenge};

const SECRET_KEY: [u8; 16] = [
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
    0x0F,
];

#[tokio::main]
async fn main() {
    // Initialize the logger
    env_logger::Builder::new().filter_level(log::LevelFilter::max()).init();

    let mut stream = match TcpStream::connect("127.0.0.1:4242").await {
        Ok(stream) => stream,
        Err(e) => {
            log::error!("Failed to connect: {e}");
            return;
        }
    };

    log::info!("Connected to server");

    // Wait for the soket to be ready for writing
    stream.writable().await.unwrap();
    let hello_server = ClientHello {
        client_version: "0.1.0".to_string(),
        client_uuid: uuid::Uuid::new_v4().to_string(),
        protocol_version: "1".to_string(),
    };

    let buffer: Vec<u8> = hello_server.encode_to_vec();
    match AsyncWriteExt::write_all(&mut stream, &buffer).await {
        Ok(_) => {
            log::info!("Sent: {:?}", hello_server);
        }
        Err(e) => {
            log::error!("Failed to send: {e}");
            return;
        }
    }

    // Wait for the socket to be ready for reading
    stream.readable().await.unwrap();
    let mut buffer = [0; 1024];
    let mut server_challenge = ServerChallenge::default();
    match stream.read(&mut buffer).await {
        Ok(n) => {
            if n == 0 {
                log::info!("Server disconnected");
                return;
            }
            log::info!("Received {n} bytes from server");
            // Decode the message
            server_challenge = match ServerChallenge::decode(&buffer[..n]) {
                Ok(hello_client) => hello_client,
                Err(e) => {
                    log::error!("Failed to decode message: {e}");
                    return;
                }
            };

            log::info!("Received: {:?}", server_challenge);
        }
        Err(e) => {
            log::error!("Failed to read from stream: {e}");
            return;
        }
    }

    // Calculate the challenge response
    let challenge = server_challenge.challenge_nonce;
    let response = match calculate_challenge_response(&challenge, &SECRET_KEY) {
        Ok(response) => response,
        Err(e) => {
            log::error!("Failed to calculate challenge response: {e}");
            return;
        }
    };
    log::info!("Challenge response: {:?}", response);

    stream.writable().await.unwrap();
    let client_response = ClientResponse {
        challenge_response: response,
    };

    let buffer: Vec<u8> = client_response.encode_to_vec();
    match AsyncWriteExt::write_all(&mut stream, &buffer).await {
        Ok(_) => {
            log::info!("Sent: {:?}", client_response);
        }
        Err(e) => {
            log::error!("Failed to send: {e}");
            return;
        }
    }

    // Launch the engine and create a window (this will block until the window is closed)
    let mut engine = EngineClient::new();
    engine.initialize("Void Architect Client", 1280, 720);
    match engine.run() {
        Ok(_) => {}
        Err(e) => {
            log::error!("Engine run failed: {e}");
            return;
        }
    }
    engine.shutdown();

    stream.shutdown().await.unwrap();
    log::info!("Closing client");
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
