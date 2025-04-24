//!
//! Integration-like tests for client logic (no real network)
//!
use void_architect_shared::messages::{HelloServer, HelloClient};
use prost::Message;

#[test]
fn hello_server_encode_decode_nominal() {
    let msg = HelloServer { client_info: "TestClient".to_string() };
    let bytes = msg.encode_to_vec();
    let decoded = HelloServer::decode(&*bytes).expect("decode failed");
    assert_eq!(decoded.client_info, "TestClient");
}

#[test]
fn hello_client_decode_invalid_data_fails() {
    let bytes = [0x00, 0xFF, 0xAA];
    let result = HelloClient::decode(&bytes[..]);
    assert!(result.is_err());
}

#[test]
fn hello_server_encode_decode_empty_field() {
    let msg = HelloServer { client_info: String::new() };
    let bytes = msg.encode_to_vec();
    let decoded = HelloServer::decode(&*bytes).expect("decode failed");
    assert_eq!(decoded.client_info, "");
}