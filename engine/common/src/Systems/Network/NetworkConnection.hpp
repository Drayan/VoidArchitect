//
// Created by Michael Desmedt on 16/07/2025.
//
#pragma once

#include "VoidArchitect/Engine/Common/Core.hpp"

#include "NetworkTypes.hpp"

namespace VoidArchitect::Network
{
    /// @brief Interface representing a single point-to-point network connection
    ///
    /// `INetworkConnection` provides a unified interface for bidirectional
    /// communication between two endpoints. This abstraction allows both
    /// client and server systems to use consistent APIs while supporting
    /// different underlying implementations (`GameNetworkingSockets`, `WebSockets`, etc.).
    ///
    /// **Design principles: **
    /// - Transport agnostic (supports multiple networking libraries)
    /// - Thread-safe message passing via internal queuing
    /// - Event-driven callbacks with implementation-dependent execution
    /// - Reliability control for performance optimization
    ///
    /// **Usage contexts: **
    /// - **Client side**: Single connection to Gateway server
    /// - **Server side**: Individual client connections managed by listener
    ///
    /// **Threading model: **
    /// - Message sending: Thread-safe, queued for background transmission
    /// - Message callbacks: Implementation-dependent execution context
    /// - ProcessEvents(): Must be called from main thread regularly
    ///
    /// **Lifecycle: **
    /// 1. Connection established (externally via client connect or server accept)
    /// 2. Set message and status handlers
    /// 3. Send/receive messages during active session
    /// 4. Handle disconnection events
    /// 5. Clean-up via Disconnect() or destructor
    ///
    /// @see `ClientNetworkSystem` Usage in client applications
    /// @see `GatewayNetworkSystem` Usage in server applications
    class VA_API INetworkConnection
    {
    public:
        virtual ~INetworkConnection() = default;

        // === Connection State Management ===

        /// @brief Check if a connection is established and operational
        /// @return True, if a connection is active and can send/receive messages
        ///
        /// A connected state indicates the connection is ready for bidirectional
        /// communication. This includes successful handshake completion and
        /// any required authentication or initialization steps.
        ///
        /// **State transitions: **
        /// - Initial: false (connection not established)
        /// - Connected: true (ready for communication)
        /// - Disconnected: false (connection lost or closed)
        virtual bool IsConnected() const = 0;

        /// @brief Gracefully close the connection
        ///
        /// Initiates a graceful shutdown of the connection, allowing any
        /// pending messages to be sent before closing. After calling this
        /// method, `IsConnected()` will return false and no further communication
        /// is possible.
        ///
        /// **Implementation notes:**
        /// - Should be safe to call multiple times
        /// - May trigger status change callback with connected=false
        /// - Pending outgoing messages may or may not be sent
        virtual void Disconnect() = 0;

        /// @brief Get detailed connection statistics for monitoring
        /// @return Current connection statistics and performance metrics
        ///
        /// Provides comprehensive metrics for network performance analysis,
        /// debugging, and monitoring. Statistics have been cumulative since
        /// the connection establishment and updated in real-time.
        ///
        /// **Usage examples: **
        /// - Performance monitoring dashboards
        /// - Connection quality indicators in UI
        /// - Network debugging and diagnostics
        /// - Bandwidth usage tracking
        virtual ConnectionStats GetStats() const = 0;

        // === Message Communication ===

        /// @brief Send a message with a specified reliability guarantee
        /// @param message Message content as string (typically JSON)
        /// @param reliability Delivery reliability mode
        ///
        /// Queues a message for transmission to the remote endpoint. Messages
        /// are sent asynchronously by background threads without blocking
        /// the calling thread. Reliability mode controls delivery guarantees.
        ///
        /// **Reliability modes: **
        /// - Unreliable: Fast, no delivery guarantee (position updates)
        /// - Reliable: Guaranteed delivery with ordering (commands, auth)
        ///
        /// **Thread safety: ** Safe to call from any thread
        /// **Performance: ** Non-blocking, queues message for background sending
        virtual void SendMessage(const std::string& message, Reliability reliability) = 0;

        // === Event Handling ===
        //TODO: This need a real EventSystem to work properly.
    };
}
