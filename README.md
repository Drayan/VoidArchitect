# VoidArchitect Engine

VoidArchitect is a modern, modular C++ game engine designed for creating high-performance 4X space
strategy games and MMORPGs. Built with a focus on scalability, cross-platform compatibility, and
cutting-edge rendering technology.

## 🚀 Key Features

### Core Engine Architecture

- **🏗️ Modular Design**: Clean separation between engine, client, editor, and server components
- **🌐 Cross-Platform**: Native support for Windows, Linux, and macOS
- **⚡ High-Performance Rendering**: Vulkan-based RHI (Rendering Hardware Interface) with API
  abstraction
- **🧵 Multithreaded Job System**: Advanced task scheduling with dependency management and SyncPoints
- **📦 Resource Management**: Comprehensive asset pipeline with materials, meshes, textures, and
  shaders

### Rendering System

- **🎨 Modern Rendering Pipeline**: Vulkan-first architecture with RenderGraph support
- **🔧 API-Agnostic Design**: Platform/RHI abstraction layer for future graphics API support
- **🎭 Material System**: Flexible material templates and shader management
- **📐 Geometry Management**: Advanced mesh handling with submesh support
- **🖼️ Texture System**: Complete 2D texture pipeline with compression support

### Development Tools

- **🔨 Built-in Editor**: Integrated level editor for content creation
- **📊 Advanced Logging**: Comprehensive logging system with spdlog integration
- **🧪 Testing Framework**: Unit testing support across all modules
- **📖 Documentation**: Doxygen-based auto-documentation system

## 🏛️ Architecture

### Project Structure

```
VoidArchitect/
├── engine/          # Core engine library
│   ├── src/
│   │   ├── Core/            # Foundation systems (Logger, Collections, Math)
│   │   ├── Platform/        # Platform abstraction layer
│   │   │   └── RHI/         # Rendering Hardware Interface
│   │   │       └── Vulkan/  # Vulkan implementation
│   │   ├── Systems/         # Engine systems (Renderer, Jobs, Materials)
│   │   └── Resources/       # Asset management
├── client/          # Game client application
├── editor/          # Content creation tools
├── server/          # Dedicated server application
├── tests/           # Tests suite
└── docs/            # Documentation
```

### Core Systems

#### Rendering Hardware Interface (RHI)

- **🎯 Purpose**: API-agnostic rendering abstraction
- **🔒 Design Principle**: Vulkan code isolated to `Platform/RHI/Vulkan`
- **📋 Features**:
    - Render state management
    - Material binding
    - Mesh rendering
    - Render target management
    - Command buffer abstraction

#### Job System

- **🧵 Architecture**: Lock-free task scheduler with dependency graphs
- **⚡ Features**:
    - SyncPoint-based dependency management
    - Priority-based scheduling
    - Continuation support
    - Thread-safe operations
    - Automatic workload distribution

#### Resource Systems

- **🎨 Material System**: Template-based material creation and management
- **📐 Mesh System**: Geometry handling with submesh support
- **🖼️ Texture System**: 2D texture loading and management
- **🔧 Shader System**: Shader compilation and binding

## 🛠️ Technologies

### Core Dependencies

- **🪟 SDL3**: Cross-platform window management and event handling
- **🎨 Vulkan**: Modern graphics API with MoltenVK for macOS support
- **📝 spdlog**: High-performance logging library
- **📦 Assimp**: 3D model loading and processing
- **⚙️ yaml-cpp**: Configuration file parsing
- **📐 GLM**: OpenGL Mathematics library for 3D math
- **🗜️ LZ4**: Fast compression/decompression
- **🖼️ STB**: Image loading utilities
- **🧵 ConcurrentQueue**: Lock-free concurrent data structures

### Build System

- **🔨 CMake**: Cross-platform build configuration
- **🧪 Testing**: Integrated unit testing framework
- **📖 Documentation**: Doxygen integration for API documentation
- **🔄 PCH**: Precompiled headers for faster compilation

## 🚧 Development Status

### ✅ Implemented Features

- Cross-platform window initialization and event loop
- Vulkan-based rendering system with RHI abstraction
- Comprehensive job system with dependency management
- Material, mesh, and texture management systems
- Logging and debugging infrastructure
- Core math and collection utilities
- Memory-efficient resource handles
- Render state and pass management

### 🎯 Current Focus Areas

- RenderGraph implementation for automatic pass ordering
- Extended material template system
- Asset loading pipeline optimization
- Editor integration and tooling
- Performance profiling and optimization

### 📋 Planned Features

See [ROADMAP.md](ROADMAP.md) for detailed development plans including:

- Additional rendering backends (DirectX 12, OpenGL, Metal)
- Physics integration
- Audio system
- Networking for MMO support
- Procedural content generation
- Advanced lighting and post-processing

## 🏗️ Building

### Prerequisites

- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2022+)
- CMake 3.20+
- Vulkan SDK
- Platform-specific dependencies via package manager

### Quick Start

```bash
# Clone the repository
git clone https://github.com/YourUsername/VoidArchitect.git
cd VoidArchitect

# Configure build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release

# Run
./bin/VoidArchitect_Client
```

### Configuration Options

```cmake
-DVOID_ARCHITECT_BUILD_TESTS=ON    # Enable unit tests
-DVOID_ARCHITECT_BUILD_EDITOR=ON   # Build editor application
-DVOID_ARCHITECT_BUILD_SERVER=ON   # Build dedicated server
```

## 🤝 Contributing

VoidArchitect follows a design-first development approach:

1. **🎨 Design Phase**: Architectural discussion and feature planning
2. **⚡ Implementation Phase**: Code implementation with comprehensive documentation
3. **🧪 Testing**: Unit tests and integration validation
4. **📖 Documentation**: Doxygen comments and user guides

### Code Style

- **📝 Documentation**: Complete Doxygen documentation for all public and private APIs
- **🔤 Comments**: English only for code comments
- **🏗️ Architecture**: Respect API abstraction boundaries (no Vulkan outside Platform/RHI/Vulkan)
- **🧪 Testing**: Unit tests for core functionality

## 📜 License

Licensed under the Apache License 2.0. See [LICENSE](LICENSE) for details.

## 🔗 Links

- [🗺️ Roadmap](ROADMAP.md) - Development roadmap and planned features
- [📖 Documentation](docs/) - API documentation and guides
- [🐛 Issues](https://github.com/Drayan/VoidArchitect/issues) - Bug reports and feature
  requests
- [💬 Discussions](https://github.com/Drayan/VoidArchitect/discussions) - Community discussions

---

**VoidArchitect Engine** - Building the foundation for next-generation space strategy games.