# VoidArchitect

VoidArchitect is a modular, C++-based project aiming to create a 4X (eXplore, eXpand, eXploit,
eXterminate) space strategy game, powered by a custom game engine.

## Architecture

The project is organized into several subprojects:

- **engine**: The cross-platform engine subproject, responsible for system management, such as
  platform abstraction, event system, inputs systems, rendering... Uses `SDL3` for window and event
  loop management.
- **editor**: Editor version of the client, used internally to "edit" the gameworld, materials, etc.
- **client**: The main client application, which instantiates and runs the engine. It should provide
  Game 'hooks' callbacks that will be used by the engine at some specific points.
- **server**: The main server application, which instantiates and runs the server.

## Key Features

- **Cross-platform window initialization** via `SDL3` (Windows, Linux, macOS supported)
- **Native event loop** integrated into the client application
- **Clear separation** between engine (engine_client) and application logic (client) for testability
  and scalability

## Key Technologies

## License

Apache 2.0