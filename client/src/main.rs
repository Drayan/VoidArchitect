use prost::Message;
use tokio::{io::AsyncWriteExt, net::TcpStream};
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

    loop {
        // Wait for the soket to be ready for writing
        stream.writable().await.unwrap();
        let hello_client = protos::HelloClient {
            server_info: "4242".to_string(),
        };

        let buffer: Vec<u8> = hello_client.encode_to_vec();
        match AsyncWriteExt::write_all(&mut stream, &buffer).await {
            Ok(_) => {
                println!("Sent: {:?}", hello_client);
            }
            Err(e) => {
                println!("Failed to send: {e}");
                break;
            }
        }

        break;
    }

    stream.shutdown().await.unwrap();
    println!("Closing client");
}
