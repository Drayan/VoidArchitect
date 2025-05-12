# VoidArchitect

VoidArchitect is a modular, C++-based project aiming to create a 4X (eXplore, eXpand, eXploit, eXterminate) space strategy game, powered by a custom game engine.

## Architecture

The project is organized into several subprojects:

- **engine_client**: The cross-platform client-side engine subproject, responsible for system management, such as platform abstraction, event system, inputs systems, rendering... Uses `SDL3` for window and event loop management.
- **engine_server**: The macOS/linux server-side engine subproject, it should start every server system, such as network connections, client sessions, ...
- **client**: The main client application, which instantiates and runs the engine. It should provide Game 'hooks' callbacks that will be used by the engine at some specific points. 
- **server**: The main server application, which instantiates and runs the server.
- **shared**: Expose datastructures, systems, and algorithms that are shared between the client and the engine.

## Key Features

- **Cross-platform window initialization** via `SDL3` (Windows, Linux, macOS supported)
- **Native event loop** integrated into the client application
- **Clear separation** between engine (engine_client) and application logic (client) for testability and scalability

## Key Technologies

- **Languages:** Rust (Server)
- **Build:** Cargo (Server)
- **Communication:** Protobuf v3, TCP/IP
- **Server Networking (Planned):** Tokio (Async Runtime)
- **Client Graphics (Planned):** Low-level API (Vulkan, Metal, DirectX12)

## License
Apache 2.0