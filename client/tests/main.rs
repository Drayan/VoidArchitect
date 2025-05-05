//!
//! Integration-like tests for client logic (no real network)
//!
use prost::Message;
use void_architect_shared::messages::{
    ClientHello, ClientResponse, HandshakeFailure, HandshakeSucess, ServerChallenge,
};

#[test]
fn client_hello_encode_decode_nominal() {
    // Test encoding/decoding ClientHello (message sent by client)
    let msg = ClientHello {
        client_version: "0.1.0".to_string(),
        protocol_version: "1".to_string(),
        client_uuid: "client-uuid-456".to_string(),
    };
    let bytes = msg.encode_to_vec();
    let decoded = ClientHello::decode(&*bytes).expect("decode failed");
    assert_eq!(decoded.client_version, "0.1.0");
    assert_eq!(decoded.protocol_version, "1");
    assert_eq!(decoded.client_uuid, "client-uuid-456");
}

#[test]
fn server_challenge_decode_invalid_data_fails() {
    // Test decoding invalid bytes using ServerChallenge (message received by client)
    let bytes = [0x00, 0xFF, 0xAA];
    let result = ServerChallenge::decode(&bytes[..]);
    assert!(result.is_err());
}

#[test]
fn client_response_encode_decode_nominal() {
    // Test encoding/decoding ClientResponse (message sent by client)
    let msg = ClientResponse {
        challenge_response: vec![1, 3, 3, 7],
    };
    let bytes = msg.encode_to_vec();
    let decoded = ClientResponse::decode(&*bytes).expect("decode failed");
    assert_eq!(decoded.challenge_response, vec![1, 3, 3, 7]);
}

#[test]
fn handshake_success_decode_nominal() {
    // Test decoding HandshakeSucess (message received by client)
    let msg = HandshakeSucess {
        // Using typo'd name from proto
        session_id: 9876,
        server_name: Some("AnotherServer".to_string()),
        game_version: None,
    };
    let bytes = msg.encode_to_vec(); // Encode first to get bytes
    let decoded = HandshakeSucess::decode(&*bytes).expect("decode failed");
    assert_eq!(decoded.session_id, 9876);
    assert_eq!(decoded.server_name, Some("AnotherServer".to_string()));
    assert_eq!(decoded.game_version, None);
}
