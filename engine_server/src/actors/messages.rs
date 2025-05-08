use actix::Message;
use std::net::TcpStream;

#[derive(Message)]
#[rtype(result = "()")]
pub struct StreamTransfer {
    pub stream: TcpStream,
}
