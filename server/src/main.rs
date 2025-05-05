//! Main entry point for the VoidArchitect server application.
//!
//! This binary listens for TCP connections from clients, performs a handshake using Protobuf messages,
//! and demonstrates basic async networking and logging. Game state and logic will be added in future milestones.

use void_architect_engine_server::EngineServer;

// const MAX_MESSAGE_SIZE: usize = 1024;

fn main() {
    let engine = match EngineServer::new() {
        Ok(engine) => engine,
        Err(e) => {
            log::error!("Failed to initialize engine server: {e:#?}");
            return;
        }
    };

    match engine.run() {
        Ok(_) => log::info!("Engine server stopped successfully."),
        Err(e) => log::error!("Engine server encountered an error: {e:#?}"),
    }
}

// async fn handle_connection(mut stream: tokio::net::TcpStream) {
//     // The current handshade protocol is a simple ping-pong between
//     // the client and the server. The client need to send a HelloServer
//     // message and the server will reply with a HelloClient message.
//     log::info!("Waiting for client...");
//     let hello_server: HelloServer;
//     match stream.readable().await {
//         Ok(_) => {
//             let mut buffer = [0; MAX_MESSAGE_SIZE];
//             // Wait for the socket to be ready for reading
//             match stream.read(&mut buffer).await {
//                 Ok(n) => {
//                     if n == 0 {
//                         log::info!("Client disconnected");
//                         return;
//                     }
//                     log::info!("Received {n} bytes from client");

//                     // Decode the message
//                     hello_server = match HelloServer::decode(&buffer[..n]) {
//                         Ok(hello_server) => hello_server,
//                         Err(e) => {
//                             log::error!("Failed to decode message: {e:#?}");
//                             return;
//                         }
//                     };
//                     log::info!("Received: {hello_server:#?}");
//                 }
//                 Err(e) => {
//                     log::error!("Failed to read from stream: {e:#?}");
//                     return;
//                 }
//             }
//         }
//         Err(e) => {
//             log::error!("Failed to read from stream: {e:#?}");
//             return;
//         }
//     }

//     // Send a HelloServer message back to the client
//     let hello_client = HelloClient {
//         server_info: hello_server.client_info,
//     };
//     let message = hello_client.encode_to_vec();
//     // Waiting for the socket to be ready for writing
//     match stream.writable().await {
//         Ok(_) => {
//             log::info!("Sending message to client");
//             match stream.write_all(&message).await {
//                 Ok(_) => {
//                     log::info!("Sent: {hello_client:#?}");
//                 }
//                 Err(e) => {
//                     log::error!("Failed to send message: {e:#?}");
//                     return;
//                 }
//             }
//         }
//         Err(e) => {
//             log::error!("Failed to write to stream: {e:#?}");
//             return;
//         }
//     }
// }
