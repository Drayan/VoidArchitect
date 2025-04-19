include!(concat!(env!("OUT_DIR"), "/core.serialization.protos.rs"));

#[tokio::main]
async fn main() {
    println!("Hello, world!");

    // Open a listener on the specified address
    let listener = tokio::net::TcpListener::bind("127.0.0.1:4242")
        .await
        .unwrap();
    println!("Listening on 127.0.0.1:4242");

    loop {
        // Accept a new connection
        let (socket, _) = listener.accept().await.unwrap();
        println!("Accepted connection from {:?}", socket);
    }
}
