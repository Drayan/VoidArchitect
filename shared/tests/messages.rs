//!
//! Unit tests for Protobuf message round-trip in shared crate
//!
use void_architect_shared::messages::{HelloServer, HelloClient};
use prost::Message;

#[test]
fn hello_server_roundtrip_nominal() {
    let msg = HelloServer { client_info: "TestClient".to_string() };
    let bytes = msg.encode_to_vec();
    let decoded = HelloServer::decode(&*bytes).expect("decode failed");
    assert_eq!(decoded.client_info, "TestClient");
}

#[test]
fn hello_server_roundtrip_empty_field() {
    let msg = HelloServer { client_info: String::new() };
    let bytes = msg.encode_to_vec();
    let decoded = HelloServer::decode(&*bytes).expect("decode failed");
    assert_eq!(decoded.client_info, "");
}

#[test]
fn hello_server_decode_invalid_data_fails() {
    let bytes = [0xFF, 0xFF, 0xFF];
    let result = HelloServer::decode(&bytes[..]);
    assert!(result.is_err());
}

#[test]
fn hello_client_roundtrip_nominal() {
    let msg = HelloClient { server_info: "TestServer".to_string() };
    let bytes = msg.encode_to_vec();
    let decoded = HelloClient::decode(&*bytes).expect("decode failed");
    assert_eq!(decoded.server_info, "TestServer");
}