use env_logger::Builder;
use log::LevelFilter;
use prost::Message;
use tokio::{io::AsyncReadExt, io::AsyncWriteExt, net::TcpListener};
use void_architect_shared::messages::{HelloClient, HelloServer};

const MAX_MESSAGE_SIZE: usize = 1024;

fn get_config() -> (String, String) {
    // TODO: Load the configuration from environment variables or a config file
    // For now, we will just return a default configuration
    let default_host = "127.0.0.1";
    let default_port = "4242";

    // If the VOID_SERVER_HOST environment variable is set, use it
    let host = match std::env::var("VOID_SERVER_HOST") {
        Ok(env_host) => env_host,
        Err(_) => default_host.to_string(),
    };

    let host = if host.is_empty() {
        default_host.to_string()
    } else {
        host
    };

    // If the VOID_SERVER_PORT environment variable is set, use it
    let port = match std::env::var("VOID_SERVER_PORT") {
        Ok(env_port) => env_port,
        Err(_) => default_port.to_string(),
    };

    let port = if port.is_empty() {
        default_port.to_string()
    } else {
        port
    };

    (host, port)
}

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
