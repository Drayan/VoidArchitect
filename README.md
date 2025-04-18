# VoidArchitect

VoidArchitect is a project aiming to create a 4X (eXplore, eXpand, eXploit, eXterminate) space strategy game, powered by a custom game engine. The project adopts a distinct client-server architecture.

## General Architecture

The project is divided into two main components:

1.  **Server (`va_server`):**

    - Written in **Rust**.
    - Responsible for the main game logic, managing the state of the universe (galaxy, systems, empires, etc.), validating player actions, and synchronizing the state between clients.
    - Designed to be **authoritative** and performant, leveraging Rust's concurrency and memory safety features.
    - Will likely use the `tokio` ecosystem for asynchronous network management.

2.  **Client (`va_client`):**

    - Written in **C++** (C++20 standard or newer).
    - Responsible for graphics rendering (via a low-level API like Vulkan, Metal, or eventually DirectX), handling user input, communicating with the server, and potentially client-side prediction.
    - Designed to be **cross-platform** (Windows, Linux, macOS).

3.  **Communication:**
    - The client and server communicate via a network protocol based on **TCP/IP**.
    - Exchanged messages are defined using **Protocol Buffers (Protobuf)** to ensure structured, efficient, and scalable communication. The `.proto` definitions are located in the `/proto` folder.

## Current Status

The project is currently in its very early stages of development (Milestone 0 / Technical Foundations).

**Immediate Goal:** Set up the basic structure for the client and server projects, configure the build systems, and establish minimal bidirectional communication between the client and server using Protobuf.

## Prerequisites

Before you can compile and run the project, you will need the following tools installed on your system:

- **Rust:** Installed via [rustup](https://rustup.rs/). (Check with `rustc --version` and `cargo --version`).
- **Modern C++ Compiler:** Supporting at least C++20 (e.g., GCC >= 7, Clang >= 5, MSVC >= 19.14 / Visual Studio 2017).
- **CMake:** Version 3.15 or newer. (Check with `cmake --version`).
- **Protobuf Compiler (`protoc`):** Version 3.x or newer. (See [installation instructions](https://grpc.io/docs/protoc-installation/)).
- **Git:** To clone the repository and manage versions.

## Compilation

### Server (`va_server` - Rust)

1.  Navigate to the server directory: `cd va_server`
2.  Compile the project in debug mode: `cargo build`
3.  (Optional) Compile the project in release mode (optimized): `cargo build --release`

The executables will be located in the `target/debug` or `target/release` subdirectory.

### Client (`va_client` - C++)

1.  Navigate to the client directory: `cd va_client`
2.  Create a separate build directory: `mkdir build && cd build`
3.  Configure the project with CMake: `cmake ..`
    - _On Windows with Visual Studio:_ You might need to specify a generator, e.g.: `cmake .. -G "Visual Studio 17 2022" -A x64`
    - _For an optimized build:_ `cmake .. -DCMAKE_BUILD_TYPE=Release`
4.  Compile the project:
    - On Linux/macOS: `make` (or `ninja` if configured)
    - With generic CMake: `cmake --build .`
    - With Visual Studio: Open the generated `.sln` solution in the `build` folder and compile from the IDE, or use `cmake --build . --config Release` for a release build from the command line.

The executables will typically be found in the `build` directory (or a subdirectory like `build/Debug` or `build/Release` depending on the generator).

## Execution

1.  **Launch the server:**

    - From the `va_server` directory: `cargo run` (for the debug version)
    - Or execute directly: `va_server/target/debug/va_server` (adjust the path if in release or on Windows `.exe`)
    - The server should indicate that it is listening on a specific port (e.g., `127.0.0.1:12345`).

2.  **Launch the client:**
    - Execute the compiled binary: `va_client/build/va_client` (adjust the path according to your system and build configuration).
    - The client should attempt to connect to the server.

Check the server and client console logs to verify communication.

## Key Technologies

- **Languages:** Rust (Server), C++20 (Client)
- **Build:** Cargo (Server), CMake (Client)
- **Communication:** Protobuf v3, TCP/IP
- **Server Networking (Planned):** Tokio (Async Runtime)
- **Client Graphics (Planned):** Low-level API (Vulkan, Metal, DirectX12)

## Contribution

(Section to be defined later, if the project becomes open source or collaborative).

## License

Apache 2.0
