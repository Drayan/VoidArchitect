# VoidArchitect

VoidArchitect is a modular, Rust-based project aiming to create a 4X (eXplore, eXpand, eXploit, eXterminate) space strategy game, powered by a custom game engine.

## Architecture

The project is organized into several Rust crates:

- **engine_client**: The cross-platform engine crate, responsible for window creation, platform abstraction, and (future) Vulkan integration. Uses `SDL3` for window and event loop management.
- **client**: The main client application crate, which instantiates and drives the engine. Acts as the user interface and entry point for client logic.
- **(Planned) server**: Will handle game logic and world state for multiplayer (not yet implemented).

## Key Features

- **Cross-platform window initialization** via `SDL3` (Windows, Linux, macOS supported)
- **Native event loop** integrated into the client application
- **Clear separation** between engine (engine_client) and application logic (client) for testability and scalability
- **Comprehensive testing strategy** including:
  - **Unit tests** integrated with source files
  - **Integration tests** in dedicated test files
  - **Property-based testing** using `proptest`
  - **Performance benchmarks** using `criterion`
  - **Mock objects** when needed via `mockall`
- **Comprehensive documentation** and rustdoc comments

## Known Limitations

- Server logic and Vulkan rendering are not yet implemented.

## Prerequisites

- Rust (edition 2021 or newer)
- Supported OS: Windows, Linux, macOS
- (Optional, for future graphics work) Vulkan SDK

## Running the Client

```sh
cargo run -p client
```

A window will open, managed by the engine via `SDL3`.

## Repository Structure

- `/engine_client` — Rust engine crate (windowing, platform abstraction)
- `/client` — Main Rust client application
- `/tests` — Unit and integration tests
- `/proto` — (Planned) Protobuf definitions for networking

## Roadmap (extract)

- [x] Cross-platform window initialization (SDL3)
- [ ] Vulkan rendering integration
- [ ] Rust server implementation
- [ ] Network protocol (Protobuf)
- [ ] 4X gameplay features

## Development Notes

- Strict adherence to Rust conventions, modularity, and testing
- See `PLANNING.md` and [TASKS.md](./TASKS.md) for architecture and progress details

The project is currently in its early development stages (Milestone 0: Technical Foundations).

**Immediate Goal:** Establish a robust Rust-based foundation for the client engine and application, with cross-platform window and event loop support. Networking, server logic, and rendering will be added in future milestones.

## Prerequisites

- **Rust:** Installed via [rustup](https://rustup.rs/). (Check with `rustc --version` and `cargo --version`).
- **Git:** To clone the repository and manage versions.
- *(Optional, for future graphics work)* **Vulkan SDK**

## Key Technologies

- **Languages:** Rust (Server)
- **Build:** Cargo (Server)
- **Communication:** Protobuf v3, TCP/IP
- **Server Networking (Planned):** Tokio (Async Runtime)
- **Client Graphics (Planned):** Low-level API (Vulkan, Metal, DirectX12)

## Contribution

(Section to be defined later, if the project becomes open source or collaborative).

## License
Apache 2.0
