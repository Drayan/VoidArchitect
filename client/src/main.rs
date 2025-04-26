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
use void_architect_shared::messages::{HelloClient, HelloServer};

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
    let hello_server = HelloServer {
        client_info: "Hello!".to_string(),
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
    match stream.read(&mut buffer).await {
        Ok(n) => {
            if n == 0 {
                log::info!("Server disconnected");
                return;
            }
            log::info!("Received {n} bytes from server");
            // Decode the message
            let hello_client: HelloClient = match HelloClient::decode(&buffer[..n]) {
                Ok(hello_client) => hello_client,
                Err(e) => {
                    log::error!("Failed to decode message: {e}");
                    return;
                }
            };

            log::info!("Received: {:?}", hello_client);
        }
        Err(e) => {
            log::error!("Failed to read from stream: {e}");
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
