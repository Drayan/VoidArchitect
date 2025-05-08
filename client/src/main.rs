//! Main entry point for the VoidArchitect client application.
//!
//! This binary connects to the server, exchanges initial handshake messages using Protobuf,
//! and launches the engine client window. It demonstrates basic networking and engine startup.

use void_architect_engine_client::EngineClient;

fn main() {
    // Initialize the logger
    env_logger::Builder::new().filter_level(log::LevelFilter::max()).init();

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

    log::info!("Closing client");
}
