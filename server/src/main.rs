include!(concat!(env!("OUT_DIR"), "/core.serialization.protos.rs"));
use env_logger::Builder;
use log::LevelFilter;
use tokio::net::TcpListener;

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
        let (socket, _) = listener.accept().await.unwrap();
        log::info!("Accepted connection from {:#?}", socket);
    }
}
