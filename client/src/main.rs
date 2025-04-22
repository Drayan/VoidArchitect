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
    env_logger::Builder::new()
        .filter_level(log::LevelFilter::max())
        .init();

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

    // Initialize the engine
    let mut engine_client = EngineClient::new();
    engine_client.initialize();
    engine_client.run();
    engine_client.shutdown();

    stream.shutdown().await.unwrap();
    log::info!("Closing client");
}
