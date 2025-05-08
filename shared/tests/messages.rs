//!
//! Unit tests for Protobuf message round-trip in shared crate
//!
use prost::Message;
use void_architect_shared::messages::{
    ClientHello,
    ClientResponse,
    HandshakeFailure,
    HandshakeSuccess,
    ServerChallenge, // Note: HandshakeSucess has a typo in proto
};

#[test]
fn client_hello_roundtrip() {
    let msg = ClientHello {
        client_version: "0.1.0".to_string(),
        protocol_version: "1".to_string(),
        client_uuid: "uuid-test-123".to_string(),
    };
    let bytes = msg.encode_to_vec();
    let decoded = ClientHello::decode(&*bytes).expect("decode failed");
    assert_eq!(decoded.client_version, "0.1.0");
    assert_eq!(decoded.protocol_version, "1");
    assert_eq!(decoded.client_uuid, "uuid-test-123");
}

#[test]
fn server_challenge_roundtrip() {
    let msg = ServerChallenge {
        server_version: "0.1.0".to_string(),
        protocol_version: "1".to_string(),
        challenge_nonce: vec![1, 2, 3, 4, 5],
    };
    let bytes = msg.encode_to_vec();
    let decoded = ServerChallenge::decode(&*bytes).expect("decode failed");
    assert_eq!(decoded.server_version, "0.1.0");
    assert_eq!(decoded.protocol_version, "1");
    assert_eq!(decoded.challenge_nonce, vec![1, 2, 3, 4, 5]);
}

#[test]
fn client_response_roundtrip() {
    let msg = ClientResponse {
        challenge_response: vec![10, 20, 30],
    };
    let bytes = msg.encode_to_vec();
    let decoded = ClientResponse::decode(&*bytes).expect("decode failed");
    assert_eq!(decoded.challenge_response, vec![10, 20, 30]);
}

#[test]
fn handshake_success_roundtrip() {
    let msg = HandshakeSuccess {
        // Using typo'd name from proto
        session_id: 12345,
        server_name: Some("TestServer".to_string()),
        game_version: Some("0.0.1".to_string()),
    };
    let bytes = msg.encode_to_vec();
    let decoded = HandshakeSuccess::decode(&*bytes).expect("decode failed");
    assert_eq!(decoded.session_id, 12345);
    assert_eq!(decoded.server_name, Some("TestServer".to_string()));
    assert_eq!(decoded.game_version, Some("0.0.1".to_string()));
}

#[test]
fn handshake_success_roundtrip_optional_empty() {
    let msg = HandshakeSuccess {
        // Using typo'd name from proto
        session_id: 67890,
        server_name: None,
        game_version: None,
    };
    let bytes = msg.encode_to_vec();
    let decoded = HandshakeSuccess::decode(&*bytes).expect("decode failed");
    assert_eq!(decoded.session_id, 67890);
    assert_eq!(decoded.server_name, None);
    assert_eq!(decoded.game_version, None);
}

#[test]
fn handshake_failure_roundtrip() {
    let msg = HandshakeFailure {
        reason: "Invalid UUID".to_string(),
    };
    let bytes = msg.encode_to_vec();
    let decoded = HandshakeFailure::decode(&*bytes).expect("decode failed");
    assert_eq!(decoded.reason, "Invalid UUID");
}

#[test]
fn decode_invalid_data_fails() {
    // Test decoding invalid bytes using one of the message types
    let bytes = [0xFF, 0xFF, 0xFF];
    let result = ClientHello::decode(&bytes[..]);
    assert!(result.is_err());
}
