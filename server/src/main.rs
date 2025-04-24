//! Main entry point for the VoidArchitect server application.
//!
//! This binary listens for TCP connections from clients, performs a handshake using Protobuf messages,
//! and demonstrates basic async networking and logging. Game state and logic will be added in future milestones.

use env_logger::Builder;
use log::LevelFilter;
use prost::Message;
use tokio::{io::AsyncReadExt, io::AsyncWriteExt, net::TcpListener};
use void_architect_server::get_config;
use void_architect_shared::messages::{HelloClient, HelloServer};

const MAX_MESSAGE_SIZE: usize = 1024;

#[tokio::main]
async fn main() {
    // Initialize the logger
    Builder::new().filter_level(LevelFilter::max()).init();

    // Open a listener on the specified address
    let (host, port) = get_config();
    log::info!("Starting server on {host}:{port}");
    // Create a TCP listener
    let listener = match TcpListener::bind(format!("{host}:{port}")).await {
        Ok(listener) => listener,
        Err(e) => {
            log::error!("Failed to bind to {host}:{port} : {e:#?}");
            return;
        }
    };

    loop {
        // Accept a new connection
        let (stream, _) = listener.accept().await.unwrap();
        log::info!("Accepted connection from {:#?}", stream.peer_addr());

        // Swap a new task to handle the connection
        tokio::spawn(handle_connection(stream));
    }
}

async fn handle_connection(mut stream: tokio::net::TcpStream) {
    // The current handshade protocol is a simple ping-pong between
    // the client and the server. The client need to send a HelloServer
    // message and the server will reply with a HelloClient message.
    log::info!("Waiting for client...");
    let hello_server: HelloServer;
    match stream.readable().await {
        Ok(_) => {
            let mut buffer = [0; MAX_MESSAGE_SIZE];
            // Wait for the socket to be ready for reading
            match stream.read(&mut buffer).await {
                Ok(n) => {
                    if n == 0 {
                        log::info!("Client disconnected");
                        return;
                    }
                    log::info!("Received {n} bytes from client");

                    // Decode the message
                    hello_server = match HelloServer::decode(&buffer[..n]) {
                        Ok(hello_server) => hello_server,
                        Err(e) => {
                            log::error!("Failed to decode message: {e:#?}");
                            return;
                        }
                    };
                    log::info!("Received: {hello_server:#?}");
                }
                Err(e) => {
                    log::error!("Failed to read from stream: {e:#?}");
                    return;
                }
            }
        }
        Err(e) => {
            log::error!("Failed to read from stream: {e:#?}");
            return;
        }
    }

    // Send a HelloServer message back to the client
    let hello_client = HelloClient {
        server_info: hello_server.client_info,
    };
    let message = hello_client.encode_to_vec();
    // Waiting for the socket to be ready for writing
    match stream.writable().await {
        Ok(_) => {
            log::info!("Sending message to client");
            match stream.write_all(&message).await {
                Ok(_) => {
                    log::info!("Sent: {hello_client:#?}");
                }
                Err(e) => {
                    log::error!("Failed to send message: {e:#?}");
                    return;
                }
            }
        }
        Err(e) => {
            log::error!("Failed to write to stream: {e:#?}");
            return;
        }
    }
}
