use tokio::net::TcpStream;
use void_architect_core::serialization::protos;

#[tokio::main]
async fn main() {
    println!("Hello, world!");

    let mut stream = match TcpStream::connect("127.0.0.1:4242").await {
        Ok(stream) => stream,
        Err(e) => {
            println!("Failed to connect: {e}");
            return;
        }
    };

    println!("Connected to server");
    println!("Stream: {stream:#?}");
}
