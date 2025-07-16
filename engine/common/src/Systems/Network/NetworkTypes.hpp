//
// Created by Michael Desmedt on 16/07/2025.
//
#pragma once

namespace VoidArchitect::Network
{
    /// @brief Connection identifier for network sessions
    ///
    /// Unique identifier used to distinguish different network connections.
    /// For client systems, it typically represents a single server connection.
    /// For server systems, represents individual client connections.
    using ConnectionID = uint32_t;

    /// @brief Invalid connection ID constant
    ///
    /// Used to indicate an invalid or uninitialized connection.
    /// No valid connection should ever have this ID.
    constexpr ConnectionID InvalidConnectionID = 0;

    /// @brief Message reliability modes for network communication
    ///
    /// Control how messages are delivered over the network, allowing
    /// optimization for different types of game data:
    /// - Unreliable: High-frequency updates that can be lost (position, animation)
    /// - Reliable: Critical messages that must arrive exactly once (commands, auth)
    enum class Reliability
    {
        /// @brief Fast delivery with no delivery guarantees
        ///
        /// Best for high-frequency updates where the latest data matters more
        /// than a delivery guarantee. Examples: player positions, animations,
        /// health updates. Provides minimal overhead and the lowest latency.
        Unreliable,

        /// @brief Guaranteed delivery with proper ordering
        ///
        /// Ensures messages are delivered exactly once and in correct order.
        /// Best for critical game events that cannot be lost. Examples:
        /// player commands, authentication, chat messages, state changes.
        /// Higher overhead but guaranteed consistency.
        Reliable,
    };

    /// @brief Connection statistics for monitoring and debugging
    ///
    /// Provides comprehensive metrics for network performance analysis,
    /// debugging connection issues, and monitoring system health.
    /// All counters have been cumulative since the connection establishment.
    struct ConnectionStats
    {
        /// @brief Total reliable messages sent since connection starts
        uint64_t reliableMessageSent = 0;

        /// @brief Total unreliable messages sent since connection starts
        uint64_t unreliableMessageSent = 0;

        /// @brief Total messages received since connection start
        uint64_t messagesReceived = 0;

        /// @brief Current round-trip time in milliseconds
        ///
        /// Measured ping time between local and remote endpoints.
        /// Updated continuously by the underlying networking library.
        uint32_t pingMs = 0;

        /// @brief Connection quality as percentage (0-100)
        ///
        /// Indicates overall connection health considering packet loss,
        /// jitter, and bandwidth utilization. 100% = perfect connection.
        float qualityPercent = 100.0f;
        std::chrono::steady_clock::duration uptime{0};
    };

    /// @brief Message handler function signature for incoming messages
    /// @param connectionId Source connection ID (client-specific on server)
    /// @param message JSON message content as string
    ///
    /// Called when a message is received from a remote endpoint. The handler
    /// is guaranteed to be executed on the main thread for thread safety.
    ///
    /// **Usage patterns: **
    /// - **Client**: connectionId typically ignored (single server connection)
    /// - **Server**: connectionId identifies which client sent the message
    /// - **Thread safety**: Always called from the main thread via JobSystem
    using MessageHandle = std::function<void(
        ConnectionID connectionId,
        const std::string& message)>;

    /// @brief Connection status change handler function signature
    /// @param connectionId Connection that changed status
    /// @param connected true if connected, false if disconnected
    ///
    /// Called when a connection status changes (connected/disconnected)
    /// Provides notification for connection lifecycle events.
    ///
    /// **Usage patterns: **
    /// - **Client**: Monitor server connection status for UI updates
    /// - **Server**: Track client connections for session management
    /// - **Thread safety**: Always called from the main thread via JobSystem.
    using ConnectionStatusHandler = std::function<void(ConnectionID connectionId, bool connected)>;

    /// @brief Client connection event handler for server systems
    /// @param connectionId Newly connected client ID
    ///
    /// Called when a new client connects to the server. Used for
    /// initializing client sessions, authentication, and state setup.
    ///
    /// **Server usage: **
    /// - Initialize client session data
    /// - Send welcome messages or authentication challenges
    /// - Update server statistics and monitoring
    using ClientConnectionHandler = std::function<void(ConnectionID connectionId)>;

    /// @brief Client disconnection event handler for server systems
    /// @param connectionId Disconnected client ID
    ///
    /// Called when a client disconnects from the server. Used for
    /// clean-up, persistence, and session state management.
    ///
    /// **Server usage: **
    /// - Clean up client session data
    /// - Save player progress
    /// - Update server statistics and monitoring
    /// - Notify other systems of player departure
    using ClientDisconnectionHandler = std::function<void(ConnectionID connectionId)>;
}
