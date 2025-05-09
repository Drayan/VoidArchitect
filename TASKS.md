# VoidArchitect Project Tasks

This document breaks down the project milestones into actionable Epics and Stories.

## Milestone 0: Basic Console Communication (Client/Server)

- **Status:** Done
- **Note:** This milestone established the foundational client-server setup.

### Epic 0.1: Establish Core Communication Backbone

- **Description:** Set up the fundamental client-server architecture using Rust, Tokio, and Protobuf for basic message exchange over TCP. Create the shared library (`void-architect_shared`) for common definitions.
- **Goal:** Have a client connect to the server, exchange a predefined message (e.g., "Hello") using Protobuf, and disconnect cleanly. The `void-architect_shared` crate is functional and used by both client and server binaries.
- **Definition of Done (DoD):**
  - Server binary listens on a configured TCP port using Tokio.
  - Client binary connects to the server's address and port using Tokio.
  - A Protobuf message definition exists for the communication handshake within `void-architect_shared/proto`.
  - The `void-architect_shared` crate compiles and provides the necessary Rust structures generated from the `.proto` file.
  - Client sends the initial message upon successful connection.
  - Server receives the message, potentially sends a reply (also Protobuf).
  - Client receives the reply (if applicable) and logs it to the console.
  - Both client and server handle basic connection/disconnection events without crashing.
  - Code is correctly placed in the `client`, `server`, and `shared` crates within the workspace.

_(No detailed stories listed with checkboxes as the milestone is complete)._

## Milestone 1: Window & Basic Vulkan Setup

- **Status:** Completed on 25-04-2025
- **Goal:** Create a desktop window capable of hosting a Vulkan surface and initialize the core Vulkan components needed for rendering, without actually drawing anything yet.

### Epic 1.1: Window Creation and Surface Integration

- **Description:** Set up a basic window using the `winit` library within the `client` application. Implement a minimal event loop and integrate the window with Vulkan by creating a `VkSurfaceKHR`.
- **Goal:** A blank window appears on the screen, managed by an event loop, and a corresponding Vulkan surface handle is successfully created and stored.
- **Definition of Done (DoD):**
  - `winit` dependency is added to the `client` crate.
  - `ash` (or chosen Vulkan wrapper) dependency includes surface extension capabilities.
  - Client `main` function initializes `winit` event loop and creates a window.
  - A window with a title (e.g., "VoidArchitect Client") is displayed.
  - A Vulkan `VkSurfaceKHR` is created using the `winit` window handle and the Vulkan instance (created in Epic 1.2).
  - The application runs the event loop and exits cleanly when the window's close button is pressed.

#### Stories:

- [x] **Story 1.1.1: Add Windowing Dependency (`winit`)**
  - **Description:** Integrate the `winit` crate into the `engine_client` project's `Cargo.toml`.
  - **Goal:** `winit` library is available for use within the client code.
- [x] **Story 1.1.2: Implement Basic Platform Abstraction Layer**
  - **Description:** Implement the basis of the `platform` system into a module in the `engine_client` crate.
  - **Goal:** A basic platform module is implemented in the `engine_client` crate.
- [x] **Story 1.1.3: Implement `Engine` struct**
  - **Description:** Define the `Engine` struct in the `engine_client` crate to hold the core state of the application, including platform and Vulkan components.
  - **Goal:** The `Engine` struct is defined and used within the `engine_client` crate.
- [x] **Story 1.1.4: Instanciate `Engine` from `client` crate**
  - **Description:** Create a new instance of the `Engine` struct in the `client` crate and initialize it.
  - **Goal:** The `Engine` instance is created and used within the `client` crate.
- [x] **Story 1.1.5: Implement Basic Window Initialization**
  - **Description:** Write code in the `engine_client` crate's dedicated module to use `winit` to create and display a desktop window.
  - **Goal:** A visible, empty window appears when the client application is executed.
  - **Note:** PlatformSystem is initialized by EngineClient, with winit event loop setup and tested.
  - **Completed:** 22-04-2025
- [x] **Story 1.1.6: Implement Minimal Event Loop**
  - **Description:** Set up the `winit` event loop to process window events, specifically handling the close request to allow clean application termination.
  - **Goal:** The window remains open and responsive until the user closes it, at which point the application exits gracefully.
  - **Completed:** 22-04-2025
- [x] **Story 1.1.7: Create Vulkan Surface (`VkSurfaceKHR`)**
  - **Description:** Using the created `winit` window and the Vulkan instance (from Epic 1.2), create the `VkSurfaceKHR` object required for presentation. This involves platform-specific Vulkan extension calls, likely handled via `ash`.
  - **Goal:** A valid `VkSurfaceKHR` handle is obtained and stored, linking the window to the Vulkan API.
  - **Completed:** 24-04-2025

### Epic 1.2: Core Vulkan API Initialization

- **Description:** Initialize the core Vulkan API objects within the `engine_client` library. This includes creating the Vulkan instance, selecting a physical device suitable for rendering to the created surface, creating a logical device, and identifying necessary queue families.
- **Goal:** Obtain and store valid handles for the Vulkan instance, physical device, logical device, and relevant queues (graphics, presentation), ready for subsequent Vulkan operations.
- **Definition of Done (DoD):**
  - `ash` (or chosen Vulkan wrapper) dependency added to `engine_client`.
  - A `VkInstance` is successfully created, enabling necessary extensions (e.g., surface extensions, potentially debug utils) and layers (e.g., validation layers in debug builds).
  - A `VkPhysicalDevice` is enumerated and selected that supports required features (graphics operations) and can present to the `VkSurfaceKHR` created in Epic 1.1.
  - Queue family indices for graphics operations and presentation support are identified on the selected physical device.
  - A `VkDevice` (logical device) is created from the physical device, enabling required device features and requesting the identified queue families.
  - Handles to the requested `VkQueue`s are obtained from the logical device.
  - All obtained Vulkan handles (Instance, PhysicalDevice, Device, Queues, Surface) are stored appropriately within an engine context struct in `engine_client`.
  - Basic error handling wraps Vulkan API calls.

#### Stories:

- [x] **Story 1.2.1: Add Vulkan Wrapper Dependency (`ash`)**
  - **Description:** Add the `ash` crate to the `engine_client` project's `Cargo.toml` and configure necessary Vulkan SDK linking if needed.
  - **Goal:** The `ash` library is available for use within the `engine_client` code.
- [x] **Story 1.2.2: Create Vulkan Instance (`VkInstance`)**
  - **Description:** Write code within `engine_client` to initialize the Vulkan loader and create a `VkInstance`. Enable required instance extensions (fetched from `winit` for the surface) and validation layers for debugging.
  - **Goal:** A valid `VkInstance` handle is created and stored.
  - **Completed:** 24-04-2025
- [x] **Story 1.2.3: Select Suitable Physical Device (`VkPhysicalDevice`)**
  - **Description:** Implement logic to enumerate available physical devices, check their properties, features, and queue families, and select one that supports graphics rendering and presentation to the created `VkSurfaceKHR`.
  - **Goal:** A valid `VkPhysicalDevice` handle representing the chosen GPU is selected and stored.
  - **Completed:** 25-04-2025
- [x] **Story 1.2.4: Identify Required Queue Families**
  - **Description:** Query the selected `VkPhysicalDevice` for its queue family properties and find the indices of queue families that support both graphics commands and presentation to the specific `VkSurfaceKHR`. Handle cases where one queue family supports both or separate families are needed.
  - **Goal:** Indices for the graphics and present queue families are identified and stored.
  - **Completed:** 25-04-2025
- [x] **Story 1.2.5: Create Logical Device and Queues (`VkDevice`, `VkQueue`)**
  - **Description:** Create the `VkDevice` (logical device interface) from the selected `VkPhysicalDevice`. Request the necessary device features (none specific yet) and specify the creation of queues from the identified families. Obtain the `VkQueue` handles.
  - **Goal:** Valid `VkDevice` and `VkQueue` handles for graphics and presentation are created and stored.
  - **Completed:** 25-04-2025

## Milestone 2: Static Vulkan Triangle

- **Status:** Completed on 02/05/2025  
- **Goal:** Render a single, static, colored triangle to the window using the Vulkan setup from Milestone 1. This involves setting up the swapchain, render pass, graphics pipeline, command buffers, vertex buffers, and the main render loop.

### Epic 2.1: Swapchain and Frame Rendering Setup

- **Description:** Create the Vulkan swapchain for presenting rendered images to the window surface. Set up related resources like image views, a basic render pass, and framebuffers.
- **Goal:** A functional swapchain is created, along with the necessary image views and framebuffers linked to a render pass, ready to be targeted by rendering commands.
- **Definition of Done (DoD):**
  - A `VkSwapchainKHR` is created based on surface capabilities, desired image count, format, and present mode.
  - Handles to the `VkImage`s owned by the swapchain are retrieved.
  - A `VkImageView` is created for each swapchain image.
  - A basic `VkRenderPass` is defined, describing the attachments (a single color attachment compatible with the swapchain format) and a single subpass using that attachment.
  - A `VkFramebuffer` is created for each `VkImageView`, associating it with the `VkRenderPass` and defining the render area dimensions.
  - Swapchain creation and management logic resides primarily in `engine_client`.

#### Stories:

- [x] **Story 2.1.1: Query Swapchain Support Details**
  - **Description:** Query the physical device for its capabilities regarding the specific window surface (supported formats, present modes, image counts, extent).
  - **Goal:** Determine suitable parameters for swapchain creation based on hardware/surface support.
  - **Completed:** 25-04-2025
- [x] **Story 2.1.2: Create Swapchain (`VkSwapchainKHR`)**
  - **Description:** Create the `VkSwapchainKHR` object using the selected parameters, logical device, and surface.
  - **Goal:** A valid `VkSwapchainKHR` handle is obtained.
  - **Completed:** 25-04-2025
- [x] **Story 2.1.3: Create Swapchain Image Views**
  - **Description:** Retrieve the `VkImage` handles from the created swapchain and create a corresponding `VkImageView` for each one.
  - **Goal:** A list/vector of `VkImageView` handles for the swapchain images is created and stored.
  - **Completed:** 25-04-2025
- [x] **Story 2.1.4: Define Basic Render Pass (`VkRenderPass`)**
  - **Description:** Create a `VkRenderPass` object defining a single color attachment (matching the swapchain format) that should be cleared at the start and stored at the end, used in a single subpass.
  - **Goal:** A valid `VkRenderPass` handle suitable for rendering to the swapchain images is created.
  - **Completed:** 25-04-2025
- [x] **Story 2.1.5: Create Framebuffers (`VkFramebuffer`)**
  - **Description:** Create a `VkFramebuffer` for each swapchain image view, linking it to the `VkRenderPass` and specifying the dimensions.
  - **Goal:** A list/vector of `VkFramebuffer` handles, one per swapchain image, is created and stored.
  - **Completed:** 25-04-2025

### Epic 2.A: Migrate from Winit to SDL3

- **Status:** Completed on 27-04-2025
- **Description:** Replace the winit windowing library with SDL3 across all client components. Update the platform abstraction layer, window creation, event loop, and Vulkan surface integration to work with SDL3 instead of winit.
- **Goal:** The client application should function identically but use SDL3 for window management and event handling instead of winit.
- **Definition of Done (DoD):**
  - winit dependency is removed and SDL3 dependency is added to the `engine_client` crate.
  - Platform abstraction layer is refactored to use SDL3 APIs.
  - Window creation code uses SDL3 instead of winit.
  - Event loop implementation is updated to match SDL3's polling model.
  - Vulkan surface creation code uses SDL3's surface creation methods.
  - All functionality from Milestone 1 remains working as before.

#### Stories:

- [x] **Story 2.A.1: Add SDL3 Dependency**
  - **Description:** Add the SDL3 crate to the `engine_client` project's Cargo.toml and remove winit.
  - **Goal:** SDL3 library is available for use within the client code.
  - **Completed:** 26-04-2025
- [x] **Story 2.A.2: Refactor Platform Abstraction Layer**
  - **Description:** Update the platform module in `engine_client` to use SDL3 instead of winit.
  - **Goal:** The platform layer is fully converted to use SDL3 APIs.
- [x] **Story 2.A.3: Update Window Creation Code**
  - **Description:** Modify the window initialization code to use SDL3's window creation methods.
  - **Goal:** A visible window is created using SDL3 instead of winit.
- [x] **Story 2.A.4: Reimplement Event Loop**
  - **Description:** Replace the winit event loop with SDL3's event polling mechanism.
  - **Goal:** The application properly processes window events using SDL3.
- [x] **Story 2.A.5: Update Vulkan Surface Integration**
  - **Description:** Update the code that creates a Vulkan surface from the window to use SDL3's methods.
  - **Goal:** A valid `VkSurfaceKHR` is created from the SDL3 window.
- [x] **Story 2.A.6: Test Milestone 1 Functionality**
  - **Description:** Verify that all functionality from Milestone 1 works correctly with SDL3.
  - **Goal:** The application behaves identically to before but now uses SDL3.
- [x] **Task 2.A.7: Finalize SDL3 Platform Migration** (27-04-2025)
  - **Status:** Done
  - **Description:** Review the `platform_sdl.rs` implementation, add necessary documentation and unit tests, replace the old `platform.rs` with the new SDL3 version, update all references within the `engine_client` crate, update existing tests, run checks (`cargo check`, `cargo test`, `cargo fmt`), and update project documentation (`README.md`, `PLANNING.md`).
  - **Completed:** 27-04-2025

### Epic 2.2: Minimal Graphics Pipeline

- **Description:** Create a basic Vulkan graphics pipeline configured to draw simple, colored triangles. This involves loading/compiling shaders, defining vertex input, and configuring pipeline states (viewport, rasterization, etc.).
- **Goal:** A `VkPipeline` object ready to be bound for drawing the triangle is created.
- **Definition of Done (DoD):**
  - Basic vertex and fragment shaders (GLSL) are written (e.g., vertex shader passes position through, fragment shader outputs a fixed color).
  - Shaders are compiled to SPIR-V bytecode (e.g., using `shaderc-rs` in a build script or at runtime).
  - SPIR-V bytecode is loaded into `VkShaderModule` objects.
  - Pipeline shader stages (vertex, fragment) are defined using the shader modules.
  - Vertex input state is defined (binding description for vertex buffer, attribute description for position).
  - Input assembly state is set to draw triangle lists.
  - Viewport and scissor states are defined (can be dynamic or fixed for now).
  - Rasterization state is configured (e.g., fill mode, no culling).
  - Multisample state is disabled.
  - Color blend state is configured (e.g., disabled/opaque).
  - A `VkPipelineLayout` is created (likely empty, as no descriptors/push constants are needed yet).
  - The `VkPipeline` is created using the pipeline layout, render pass, and configured states.
  - Pipeline creation logic resides primarily in `engine_client`.

#### Stories:

- [x] **Story 2.2.1: Write Basic Vertex and Fragment Shaders**
  - **Description:** Create simple GLSL shader files (`.vert`, `.frag`) for a minimal triangle rendering pipeline.
  - **Goal:** GLSL source code for vertex and fragment shaders exists in the `assets` or a dedicated shader directory.
- [x] **Story 2.2.2: Setup Shader Compilation (SPIR-V)**
  - **Description:** Integrate a shader compiler (like `shaderc-rs`) into the build process or runtime loading to convert GLSL shaders into SPIR-V bytecode.
  - **Goal:** SPIR-V bytecode can be generated from the GLSL source files.
- [x] **Story 2.2.3: Load SPIR-V into Shader Modules**
  - **Description:** Implement code to read the SPIR-V bytecode and create `VkShaderModule` objects from it.
  - **Goal:** `VkShaderModule` handles for the vertex and fragment shaders are created.
- [x] **Story 2.2.4: Configure Fixed Pipeline Stages**
  - **Description:** Define the configuration structures for vertex input, input assembly, viewport, scissor, rasterization, multisampling, and color blending states.
  - **Goal:** All necessary configuration structures for the fixed-function stages of the pipeline are populated.
- [x] **Story 2.2.5: Create Pipeline Layout (`VkPipelineLayout`)**
  - **Description:** Create an empty `VkPipelineLayout` as no external resources (descriptors) or push constants are used for this basic triangle.
  - **Goal:** A valid, empty `VkPipelineLayout` handle is created.
- [x] **Story 2.2.6: Create Graphics Pipeline (`VkPipeline`)**
  - **Description:** Assemble all the shader stages, fixed-function state configurations, pipeline layout, and render pass information to create the final `VkPipeline` object.
  - **Goal:** A valid `VkPipeline` handle for drawing the triangle is created.

### Epic 2.3: Vertex Data and Command Buffer Recording

- **Description:** Define the vertex data for a triangle, create a GPU buffer to store it, and record command buffers that contain the instructions to bind the graphics pipeline and vertex buffer, and issue the draw call within the correct render pass.
- **Goal:** Command buffers are allocated and recorded with the necessary Vulkan commands to draw the triangle.
- **Definition of Done (DoD):**
  - A simple vertex structure (e.g., containing a 2D or 3D position) is defined in Rust (potentially in `engine_client` or `shared` if reused).
  - Vertex data for a single triangle is defined in application memory.
  - A `VkBuffer` (vertex buffer) is created on the GPU with appropriate size and usage flags.
  - `VkDeviceMemory` is allocated and bound to the vertex buffer.
  - The triangle vertex data is copied from application memory to the mapped GPU memory.
  - A `VkCommandPool` is created for the graphics queue family.
  - One or more `VkCommandBuffer`s (typically one per swapchain image) are allocated from the command pool.
  - For each command buffer:
    - Recording begins.
    - The render pass begins (`vkCmdBeginRenderPass`), targeting the corresponding framebuffer.
    - The graphics pipeline is bound (`vkCmdBindPipeline`).
    - The vertex buffer is bound (`vkCmdBindVertexBuffers`).
    - A draw command is issued (`vkCmdDraw`).
    - The render pass ends (`vkCmdEndRenderPass`).
    - Recording ends.
  - Buffer creation and command recording logic resides primarily in `engine_client`.

#### Stories:

- [x] **Story 2.3.1: Define Triangle Vertex Data and Structure**
  - **Description:** Define a Rust struct for vertices (e.g., `struct Vertex { pos: [f32; 2] }`) and create an array/vector containing the data for the three vertices of a triangle.
  - **Goal:** Triangle vertex data exists in a structured format in CPU memory.
- [x] **Story 2.3.2: Create Vertex Buffer (`VkBuffer`)**
  - **Description:** Implement a function or system to create a `VkBuffer` with `VK_BUFFER_USAGE_VERTEX_BUFFER_BIT` and allocate appropriate `VkDeviceMemory` (likely host-visible for easy upload initially).
  - **Goal:** A `VkBuffer` and associated `VkDeviceMemory` for storing vertex data are created.
- [x] **Story 2.3.3: Upload Vertex Data to GPU Buffer**
  - **Description:** Map the allocated `VkDeviceMemory`, copy the triangle vertex data from the CPU array/vector into the mapped memory, and unmap it. Handle memory flushing if necessary.
  - **Goal:** The triangle vertex data resides in the GPU buffer.
- [x] **Story 2.3.4: Create Command Pool (`VkCommandPool`)**
  - **Description:** Create a `VkCommandPool` associated with the graphics queue family index, allowing allocation of command buffers for graphics operations.
  - **Goal:** A valid `VkCommandPool` handle is created.
  - **Completed:** 25-04-2025
- [x] **Story 2.3.5: Allocate Command Buffers (`VkCommandBuffer`)**
  - **Description:** Allocate the required number of command buffers (e.g., one per swapchain image) from the command pool.
  - **Goal:** `VkCommandBuffer` handles are allocated and ready for recording.
  - **Completed:** 25-04-2025
- [x] **Story 2.3.6: Record Draw Commands**
  - **Description:** Implement the logic to record the command sequence (begin render pass, bind pipeline, bind vertex buffer, draw, end render pass) into each allocated command buffer.
  - **Goal:** Command buffers contain all necessary instructions to render the triangle when submitted.

### Epic 2.4: Main Render Loop and Presentation

- **Description:** Implement the core rendering loop in the `client` application. This loop needs to acquire an image from the swapchain, submit the appropriate pre-recorded command buffer to the graphics queue, and present the rendered image back to the swapchain for display. Correct synchronization using fences and semaphores is crucial.
- **Goal:** The static triangle is continuously rendered and displayed in the window each frame.
- **Definition of Done (DoD):**
  - Synchronization primitives (`VkSemaphore` for image available, `VkSemaphore` for render finished, `VkFence` for command buffer completion) are created (likely one set per frame in flight).
  - The main application loop in `client` integrates rendering calls.
  - Inside the loop:
    - `vkAcquireNextImageKHR` is called to get the index of an available swapchain image, signaling an "image available" semaphore.
    - Wait on the fence associated with the target frame/command buffer to ensure previous submission using it is complete. Reset the fence.
    - `vkQueueSubmit` is called to submit the command buffer corresponding to the acquired image index. It waits on the "image available" semaphore and signals the "render finished" semaphore and the frame's fence.
    - `vkQueuePresentKHR` is called to present the image back to the swapchain, waiting on the "render finished" semaphore.
  - Basic handling for swapchain becoming out-of-date (e.g., due to window resize) is implemented (recreation required, though full handling might be deferred).
  - The triangle is visibly rendered and static within the client window.
  - Render loop logic resides in `client`, potentially calling functions in `engine_client`.

#### Stories:

- [x] **Story 2.4.1: Create Synchronization Primitives**
  - **Description:** Create the necessary `VkSemaphore` and `VkFence` objects required for synchronizing swapchain operations, command buffer submission, and presentation. Manage multiple sets if using multiple frames in flight.
  - **Goal:** Valid handles for required semaphores and fences are created.
- [x] **Story 2.4.2: Implement Frame Acquisition (`vkAcquireNextImageKHR`)**
  - **Description:** Call `vkAcquireNextImageKHR` within the render loop to get the next available swapchain image index and signal the appropriate semaphore.
  - **Goal:** The index of the next image to render to is obtained.
- [x] **Story 2.4.3: Implement Command Buffer Submission (`vkQueueSubmit`)**
  - **Description:** Call `vkQueueSubmit` with the correct command buffer, wait/signal semaphores, and the completion fence for the current frame in flight.
  - **Goal:** The pre-recorded command buffer is submitted to the graphics queue for execution.
- [x] **Story 2.4.4: Implement Presentation (`vkQueuePresentKHR`)**
  - **Description:** Call `vkQueuePresentKHR` to queue the rendered image for presentation to the screen, waiting on the render finished semaphore.
  - **Goal:** The completed frame is submitted for display on the window.
- [x] **Story 2.4.5: Integrate Rendering into Client Event Loop**
  - **Description:** Modify the `client`'s main event loop (`SDL3`) to continuously call the rendering functions (acquire, submit, present) during `MainEventsCleared` or `RedrawRequested` events.
  - **Goal:** Rendering happens repeatedly, displaying the triangle continuously.
- [x] **Story 2.4.6: Add Basic Swapchain Resize Handling**
  - **Description:** Detect `VK_ERROR_OUT_OF_DATE_KHR` or `VK_SUBOPTIMAL_KHR` results from acquire/present calls and trigger a (potentially placeholder) swapchain recreation process. Ensure CPU waits for GPU idle before recreating.
  - **Goal:** The application can gracefully handle (even if just by pausing rendering) window resize events that invalidate the swapchain.

### Epic 2.5: Refactor VulkanContext into RendererBackendVulkan

- **Description:** Merge the `VulkanContext` struct and its logic directly into `RendererBackendVulkan` to reduce unnecessary indirection and improve clarity, as recommended by the architectural analysis. This refactor aims to simplify the backend structure and make responsibilities more explicit.
- **Goal:** All relevant logic from `VulkanContext` is integrated into `RendererBackendVulkan`, with the resulting code modularized by feature if it approaches 500 lines, and all references, tests, and documentation updated accordingly.
- **Definition of Done (DoD):**
  - All logic from `VulkanContext` is merged into `RendererBackendVulkan`.
  - The resulting file is modularized by feature if it approaches 500 lines.
  - All references to `VulkanContext` are updated or removed.
  - All tests pass.
  - Documentation is updated to reflect the new structure.

#### Stories:

- [x] **Story 2.5.1: Plan and outline the refactor**
  - **Description:** Analyze the current responsibilities of `VulkanContext` and `RendererBackendVulkan`, and create a detailed plan for merging and modularizing the code.
  - **Goal:** A clear outline and checklist for the refactor is produced.

- [x] **Story 2.5.2: Perform the merge and modularization**
  - **Description:** Move all logic from `VulkanContext` into `RendererBackendVulkan`, splitting into modules if the file approaches 500 lines, and ensure all features are preserved.
  - **Goal:** The merge is complete, and the code is modular and maintainable.

- [x] **Story 2.5.3: Update references, tests, and documentation**
  - **Description:** Update all code references, unit and integration tests, and documentation to use the new structure. Ensure all tests pass and documentation is accurate.
  - **Goal:** The project builds, tests pass, and documentation is up to date.


## Milestone 3: Persistent Mobile Cube 

**Status** In progress. 
**Goal:** Implement basic functionality for one persistent object (a cube), managed by the server using the Actix actor model and visualized by clients. The cube's state should be loaded on server start, potentially updated by the server, saved periodically/on shutdown, and synchronized to all connected clients for rendering.

**Architecture:** Actix Actor Model (as defined in `DOCS.md`)

**Diagram: M3 Epic Dependencies**

```mermaid
graph TD
    subgraph M3 - Persistent Mobile Cube
        direction LR
        E1[Epic: Core Actor Setup] --> E2;
        E1 --> E3;
        E1 --> E4;
        E2[Epic: Game State Management (Cube)] --> E3;
        E2 --> E4;
        E3[Epic: Persistence Integration (Cube)] --> E2;
        E4[Epic: Network State Synchronization (Cube)] --> E5;
        E5[Epic: Client-Side Rendering Integration (Cube)];
    end

    style E1 fill:#f9f,stroke:#333,stroke-width:2px
    style E2 fill:#ccf,stroke:#333,stroke-width:2px
    style E3 fill:#cfc,stroke:#333,stroke-width:2px
    style E4 fill:#ffc,stroke:#333,stroke-width:2px
    style E5 fill:#fcc,stroke:#333,stroke-width:2px

```

### Epics & Stories:

**Epic 1: Core Actor Setup (Server - `engine_server`)**
*Goal: Establish the fundamental Actix actors required for session and basic world management.*

*   [x] **Story 1.1: Implement `NetworkListenerActor` Skeleton** (Completed: 2025-05-05)
    *   **Description:** Create the basic structure for the `NetworkListenerActor` responsible for accepting incoming TCP connections using Tokio and Actix.
    *   **Goal:** A functional `NetworkListenerActor` exists that can bind to a port and accept connections, ready to hand them off.
*   [x] **Story 1.2: Implement `HandshakeActor` Skeleton** (Completed: 2025-05-05)
    *   **Description:** Create the `HandshakeActor` structure. Implement the basic state machine or logic flow to handle the handshake protocol steps defined in `DOCS.md` (receiving `ClientHello`, sending `ServerChallenge`, etc.), without full validation yet.
    *   **Goal:** An actor exists that can be spawned per connection to manage the handshake sequence.
*   [x] **Story 1.3: Implement `SessionManagerActor` Skeleton** (Completed: 2025-05-05)
    *   **Description:** Create the `SessionManagerActor` responsible for receiving successful handshake results (`HandshakeResult`) and managing the lifecycle (creation, tracking, potential cleanup) of `SessionActor`s. Includes basic message handling for `HandshakeResult` and `UnregisterSession`.
    *   **Goal:** A central actor exists to manage active client sessions.
*   [x] **Story 1.4: Implement `SessionActor` Skeleton**
    *   **Description:** Create the `SessionActor` structure. Implement basic connection handling (receiving the stream/codec), placeholder message processing logic, and handling connection termination.
    *   **Goal:** An actor exists that can represent and manage the connection state for a single client.
*   [x] **Story 1.5: Implement `UniverseActor` Skeleton**
    *   **Description:** Create the `UniverseActor` structure. Define placeholder state and message handlers relevant to managing the overall game world (initially just the cube).
    *   **Goal:** An actor exists to serve as the central authority for game state.
*   [ ] **Story 1.6: Implement `PersistenceActor` Skeleton**
    *   **Description:** Create the `PersistenceActor` structure. Define placeholder message handlers for loading and saving state, without implementing the actual file I/O yet.
    *   **Goal:** An actor exists dedicated to handling state persistence operations.
*   [x] **Story 1.7: Integrate Core Actor Startup and Handoff**
    *   **Description:** Modify the server's main function to initialize the Actix system, start the `NetworkListenerActor`, `SessionManagerActor`, `UniverseActor`, and `PersistenceActor`. Implement the message passing for `NetworkListener` -> `HandshakeActor` (on connection) -> `SessionManagerActor` (on success) -> `SessionActor` (creation).
    *   **Goal:** The core actor system starts correctly, and new connections are successfully handed off to create `SessionActor` instances.

**Epic 2: Game State Management (Server - `engine_server`, `shared`)**
*Goal: Manage the state of the single persistent cube within the `UniverseActor`.*

*   [x] **Story 2.1: Define Shared Cube State Structure**
    *   **Description:** Define a Rust struct (e.g., `CubeState { id: u64, position: Vec3, rotation: Quat, color: Color }`) in the `shared` crate to represent the cube's properties. Ensure necessary math types (`glam::Vec3`, `glam::Quat`) are included and serializable if needed later (e.g., with `serde`).
    *   **Goal:** A common, shared definition of the cube's state exists, usable by both server and potentially client logic.
*   [x] **Story 2.2: Define Cube-Related Actix Messages**
    *   **Description:** Define the necessary Actix message types in `engine_server` (or `shared` if needed elsewhere) for interactions related to the cube, such as `GetCurrentCubeState` (request), `UpdateCubeState` (command), `SubscribeToCubeUpdates` (request), `CubeStateUpdate` (notification).
    *   **Goal:** Clear message contracts exist for actors to communicate about the cube's state.
*   [x] **Story 2.3: Implement Cube State Storage in `UniverseActor`**
    *   **Description:** Add state to the `UniverseActor` to hold the single `CubeState` instance. Initialize it with default values or prepare for loading (Epic 3).
    *   **Goal:** The `UniverseActor` holds the authoritative state for the cube.
*   [x] **Story 2.4: Implement Cube State Update Logic in `UniverseActor`**
    *   **Description:** Implement handlers in `UniverseActor` for messages that modify the cube's state (e.g., `UpdateCubeState`). If M3 requires automatic movement, implement an internal timer (e.g., `ctx.run_interval`) to periodically update the position/rotation and trigger notifications.
    *   **Goal:** The `UniverseActor` can modify the cube's state based on messages or internal logic.
*   [x] **Story 2.5: Implement Direct Subscription Mechanism in `UniverseActor`**
    *   **Description:** Implement the "Direct Messaging" (Option A from `DOCS.md`) notification mechanism. Add a `HashSet<Addr<SessionActor>>` to `UniverseActor` state. Implement handlers for `SubscribeToCubeUpdates` (adds sender's address to set) and potentially `Unsubscribe` (removes address). Modify state update logic (Story 2.4) to iterate the set and send `CubeStateUpdate` messages to subscribed `SessionActor`s.
    *   **Goal:** `SessionActor`s can subscribe to and receive updates about the cube's state directly from the `UniverseActor`.

**Epic 3: Persistence Integration (Server - `engine_server`)**
*Goal: Load and save the cube's state using the `PersistenceActor` and the JSON backend.*

*   [ ] **Story 3.1: Define Persistence Load/Save Messages**
    *   **Description:** Define Actix messages for persistence operations: `LoadStateRequest` (sent by `UniverseActor`), `LoadStateResponse` (sent by `PersistenceActor` with loaded state or error), and `SaveStateRequest` (sent by `UniverseActor` with state to save). Ensure the state structure (`CubeState`) is serializable/deserializable (e.g., derive `serde::Serialize`, `serde::Deserialize`).
    *   **Goal:** Clear message contracts exist for requesting state load/save operations.
*   [ ] **Story 3.2: Implement `PersistenceActor` JSON Load/Save Logic**
    *   **Description:** Implement the `handle` methods in `PersistenceActor` for `LoadStateRequest` and `SaveStateRequest`. Use `tokio::fs` for async file I/O and `serde_json` to read/write the `CubeState` (or the relevant part of the `UniverseActor`'s state containing it) to/from a defined file path (e.g., `world_state.json`). Handle file not found errors during load (e.g., return default state). Send `LoadStateResponse` back to the requester.
    *   **Goal:** The `PersistenceActor` can load and save the cube's state to a JSON file asynchronously.
*   [ ] **Story 3.3: Implement `UniverseActor` Load on Startup**
    *   **Description:** In the `UniverseActor`'s `started` method (or equivalent initialization point), send a `LoadStateRequest` message to the `PersistenceActor`'s address. Implement the handler for the `LoadStateResponse` to update the `UniverseActor`'s internal cube state upon receiving the loaded data.
    *   **Goal:** The `UniverseActor` attempts to load the cube's state from the persistence layer when it starts.
*   [ ] **Story 3.4: Implement `UniverseActor` Save Trigger**
    *   **Description:** Implement logic in `UniverseActor` to trigger saving. This could be periodic (using `ctx.run_interval`), on specific events, or during shutdown (handling `System::current().stop()`). When triggered, send a `SaveStateRequest` message with the current cube state to the `PersistenceActor`.
    *   **Goal:** The `UniverseActor` periodically or on shutdown requests the `PersistenceActor` to save the current cube state.

**Epic 4: Network State Synchronization (Server & Client - `engine_server`, `engine_client`, `shared`)**
*Goal: Synchronize the cube's state from the server to connected clients using Protobuf messages.*

*   [x] **Story 4.1: Define Protobuf Object Update Message**
    *   **Description:** Define a Protobuf message (e.g., `ObjectUpdate { uint64 object_id; Vector3 position; Quaternion rotation; ... }`) in a relevant `.proto` file (e.g., `shared/src/messages/objects.proto`). Include necessary nested types for math primitives if not already defined. Regenerate Protobuf code using `prost-build` in `shared/build.rs`.
    *   **Goal:** A Protobuf message definition exists for efficiently sending object state updates over the network.
*   [x] **Story 4.2: Implement `SessionActor` Subscription Logic**
    *   **Description:** Upon successful handshake completion and `SessionActor` startup, send the `SubscribeToCubeUpdates` message to the `UniverseActor`.
    *   **Goal:** Newly connected and authenticated clients automatically register for cube state updates.
*   [x] **Story 4.3: Implement `SessionActor` State Update Handling**
    *   **Description:** Implement the `Handler<CubeStateUpdate>` for `SessionActor`. When this message is received from `UniverseActor`, prepare the data for network transmission.
    *   **Goal:** `SessionActor` reacts to internal state update notifications.
*   [x] **Story 4.4: Implement `SessionActor` Protobuf Serialization & Sending**
    *   **Description:** Inside the `CubeStateUpdate` handler, convert the received `CubeState` data into the Protobuf `ObjectUpdate` message format. Serialize the Protobuf message using `prost`. Send the serialized bytes over the client's network connection (using the appropriate Tokio/Actix networking stream/codec).
    *   **Goal:** The server sends serialized cube state updates to the connected client.
*   [x] **Story 4.5: Implement Client Network Reception & Deserialization**
    *   **Description:** In the client's network handling logic (`engine_client` or `client`), receive the raw bytes for the `ObjectUpdate` message. Deserialize the bytes back into the Protobuf `ObjectUpdate` struct using `prost`.
    *   **Goal:** The client successfully receives and deserializes cube state updates from the server.
*   [x] **Story 4.6: Implement Client-Side State Storage/Forwarding**
    *   **Description:** Implement a mechanism on the client to store the received `ObjectUpdate` data or forward it to the rendering/game logic thread. Options include: updating a shared state variable (e.g., `Arc<Mutex<HashMap<u64, CubeState>>>`), sending it through a channel (e.g., `tokio::sync::mpsc`), or using a dedicated state management system if one exists.
    *   **Goal:** The deserialized cube state is made available to the client's rendering logic.

**Epic 5: Client-Side Rendering Integration (Client - `engine_client`, `client`)**
*Goal: Render the 3D cube on the client using the synchronized state received via the network.* (Updates existing Epic 3.3 from `TASKS.md`)

*   [x] **Story 5.1: Define Cube Mesh Data (Vertices, Indices)** (Was Story 3.3.1)
    *   **Description:** Define the vertex positions (3D) and index order for rendering a 3D cube. Store this data, potentially in the `client` or `engine_client` crate.
    *   **Goal:** Data arrays for the cube's vertices and indices exist in CPU memory.
*   [x] **Story 5.2: Create/Update GPU Buffers for Cube** (Was Story 3.3.2)
    *   **Description:** Create `VkBuffer`s and associated `VkDeviceMemory` for storing the cube's vertex and index data on the GPU. Implement logic to upload the mesh data from the CPU to these buffers.
    *   **Goal:** Cube mesh data resides in GPU buffers ready for rendering.
*   [x] **Story 5.3: Update Shaders for 3D and MVP Transforms** (Was Story 3.3.3)
    *   **Description:** Modify the vertex shader (`.vert`) to accept 3D vertex positions (`vec3`) and a Model-View-Projection (MVP) matrix (e.g., `mat4` via UBO or push constant). Calculate `gl_Position = MVP * vec4(vertex_position, 1.0)`. Modify the fragment shader (`.frag`) as needed (e.g., output fixed color). Compile shaders to SPIR-V.
    *   **Goal:** Shaders capable of rendering a transformed 3D mesh exist.
*   [x] **Story 5.4: Implement MVP Matrix Uniform (UBO or Push Constant)** (Was Story 3.3.4)
    *   **Description:** Implement the Vulkan setup to pass the MVP matrix to the vertex shader. If using UBO: define descriptor set layout, create descriptor pool/set, create UBO buffer, update pipeline layout. If using Push Constant: update pipeline layout.
    *   **Goal:** A mechanism exists to send the MVP matrix data to the GPU for use by the vertex shader.
*   [x] **Story 5.5: Update Graphics Pipeline for 3D Cube** (Was Story 3.3.5)
    *   **Description:** Recreate or update the `VkPipeline` and `VkPipelineLayout` to use the new 3D shaders, updated vertex input state description (for 3D position), the MVP uniform mechanism, and enable depth testing/writing.
    *   **Goal:** A graphics pipeline configured for rendering the transformed 3D cube with depth exists.
*   [x] **Story 5.6: Integrate Synchronized State into Render Loop** (Combines parts of 3.3.6)
    *   **Description:** In the client's render loop, access the latest cube state received from the network (from Story 4.6). Calculate the Model matrix based on the cube's position/rotation. Set up static View and Projection matrices (for a fixed camera initially). Compute the final MVP matrix.
    *   **Goal:** The MVP matrix reflecting the server's state is calculated each frame.
*   [x] **Story 5.7: Update UBO/Push Constants and Draw Commands** (Combines parts of 3.3.6)
    *   **Description:** Before recording draw commands, update the UBO buffer content or set the push constant data with the newly calculated MVP matrix. Record command buffer commands to bind the 3D cube pipeline, bind the cube's vertex/index buffers, bind descriptor sets (if using UBOs), set push constants (if used), and issue an indexed draw call (`vkCmdDrawIndexed`).
    *   **Goal:** The GPU is instructed to draw the cube using the correct mesh data and the latest transformation matrix.

## Milestone 4: ECS Framework Implementation (Phase 1 - Archetype Based Foundational Framework)

- **Status:** Planned (09-05-2025)
- **Goal:** Implement the foundational core of a custom, archetype-based Entity Component System (ECS) inspired by Bevy's ergonomics. This ECS will serve as a general-purpose data and logic management framework for various engine systems. Phase 1 focuses on core primitives, a basic **Registry** API, a simple query mechanism, global resources, and a basic **Operation** runner.
- **Added:** 09-05-2025

### Epic 4.1: ECS Core Primitives and Registry API

- **Description:** Define the fundamental ECS types (`Entity`, `Component` trait, `Registry`) and implement the initial `Registry` API for entity and component manipulation using an archetype-based storage model. This API should be generic and not tied to any specific game feature like scene representation at this stage.
- **Goal:** A `Registry` struct exists that can spawn entities, add/remove/get components for entities, and despawn entities. Component data is stored in archetypes. The API is designed for general engine use.
- **Definition of Done (DoD):**
  - `Entity` struct (ID + generation) is defined.
  - `Component` trait (`'static + Send + Sync`) is defined.
  - `Registry` struct is defined, capable of managing archetypes internally.
  - `registry.spawn() -> Entity` is implemented.
  - `registry.insert(Entity, (ComponentA, ...))` is implemented, moving entities to appropriate archetypes.
  - `registry.get::<C>(Entity) -> Option<&C>` and `registry.get_mut::<C>(Entity) -> Option<&mut C>` are implemented, accessing component data from archetypes.
  - `registry.remove::<C>(Entity)` is implemented, moving entities to appropriate archetypes.
  - `registry.despawn(Entity)` is implemented, removing entities and their components from archetypes.
  - Basic unit tests cover all API functions for generic data.
  - Decision on location: `core` crate or a new `void_architect_ecs` crate.

#### Stories:

- [x] **Story 4.1.1: Define `Entity` Identifier, `Component` Trait, and `#[derive(Component)]` Macro**
    *   **Status:** Completed on 09-05-2025
    *   **Description:** Establish the basic data structures for uniquely identifying entities, for defining components, and provide an ergonomic derive macro for the `Component` trait. The `Entity` ID should support reuse. The `Component` trait and derive macro should enforce necessary bounds (`'static + Send + Sync`) for storage and thread safety.
    *   **Goal:**
        *   An `Entity` struct is defined (e.g., `u32` index + `u32` generation), with standard derive traits (`Copy`, `Hash`, `Eq`, etc.).
        *   A public marker trait `pub trait Component: 'static + Send + Sync {}` is defined.
        *   A procedural derive macro `#[derive(Component)]` is implemented in a separate macro crate (e.g., `void_engine_macros` or `void_ecs_macros`).
        *   The derive macro correctly implements the `Component` trait for user-defined structs and ensures they meet the required bounds.
        *   The main ECS module (e.g., in `core::ecs` or a `void_ecs` crate) re-exports `Entity`, `Component`, and the `Component` derive macro.
    *   **Tasks:**
        *   Implement the `Entity` struct (e.g., in `core/src/ecs/entity.rs`).
        *   Define the `pub trait Component: 'static + Send + Sync {}` (e.g., in `core/src/ecs/component.rs`).
        *   Create a new crate for procedural macros (e.g., `void_engine_macros` or `void_ecs_macros`) with `proc-macro = true` and `syn`, `quote` dependencies.
        *   Implement the `#[derive(Component)]` procedural macro in the new macro crate.
        *   Ensure the main ECS module (e.g., `core::ecs`) depends on the macro crate and correctly re-exports the derive macro.
        *   Add basic documentation for `Entity`, `Component` trait, and usage of `#[derive(Component)]`.
        *   Add basic unit tests for `Entity` and for the `#[derive(Component)]` macro (success and failure cases for bounds).

- [x] **Story 4.1.2: Implement Archetype Storage Foundation (Internal to `Registry`)**
    *   **Status:** Completed on 09/05/2025
    *   **Description:** Design and implement the internal data structures within the `Registry` to store components based on archetypes. This includes how archetypes are identified, created, and how component data is stored within them (e.g., `Vec<T>` for each component type in an archetype). This story focuses on the *storage* aspect, not yet the public API for manipulation.
    *   **Goal:**
        *   The `Registry` struct has internal mechanisms to manage a collection of `Archetype`s.
        *   Each `Archetype` can store multiple types of components in a type-erased way (e.g., using `Box<dyn Any>` or similar techniques for `Vec<T>`) or via generic type parameters if a more concrete structure is chosen per archetype.
        *   A way to uniquely identify an archetype based on its set of `Component` types (e.g., using a sorted list of `TypeId`s).
        *   A mapping from `Entity` ID to its `Archetype` and its row index within that archetype.
    *   **Tasks:**
        *   Define an `ArchetypeId` (e.g., based on `TypeId`s of components).
        *   Define an `Archetype` struct/enum that can hold columns of component data (e.g., `HashMap<TypeId, Box<dyn Any>>` where `Any` is a `Vec<C>`).
        *   Implement `Registry` struct to hold `HashMap<ArchetypeId, Archetype>` and `HashMap<Entity, (ArchetypeId, usize)>` (entity location map).
        *   Implement internal functions for creating new archetypes and potentially for moving component data between archetypes (to be used by `insert`/`remove`).

- [x] **Story 4.1.3: Implement `registry.spawn()` and `registry.despawn()`** (Completed - 09/05/2025)
    *   **Description:** Implement the public API methods on `Registry` for creating new entities (`spawn`) and removing existing entities along with all their components (`despawn`). `spawn` should ideally find or create an archetype for entities with no components initially, or handle this as a special case. `despawn` needs to correctly remove the entity from its archetype and free its ID for reuse (respecting the generation count).
    *   **Goal:**
        *   `registry.spawn() -> Entity` method is implemented, returning a new, unique `Entity` ID. It should manage a free list of entity IDs or increment a counter, and handle generation updates.
        *   `registry.despawn(Entity)` method is implemented. It removes the entity from its current archetype (if any) and marks the `Entity` ID as available for reuse with an incremented generation.
    *   **Tasks:**
        *   Implement an entity ID allocator within `Registry` (e.g., a `Vec<u32>` for generations and a free list of indices).
        *   Implement `spawn()`: allocate ID, potentially add to a "null" archetype or prepare for component insertion.
        *   Implement `despawn()`: find entity's archetype, remove its data from all component columns in that archetype, update entity location map, free ID.

- [ ] **Story 4.1.4: Implement `registry.insert(entity, components)`**
    *   **Description:** Implement the `registry.insert()` method to add one or more components to an existing entity. This is a critical operation that will involve determining the entity's new archetype (based on its existing components plus the new ones), potentially creating that new archetype if it doesn't exist, and moving all of the entity's component data from its old archetype to the new one.
    *   **Goal:**
        *   `registry.insert(Entity, (ComponentA, ComponentB, ...))` method (likely using a tuple or a bundle trait) is implemented.
        *   The method correctly identifies or creates the target archetype.
        *   Component data for the entity is moved from the old archetype to the new one.
        *   The new components are added to their respective columns in the new archetype.
        *   The entity's location (archetype and row) is updated in the `Registry`.
    *   **Tasks:**
        *   Implement logic to calculate the target `ArchetypeId` when components are added.
        *   Implement logic to transition an entity between archetypes:
            *   Copy existing component data from old to new archetype.
            *   Add new component data to new archetype.
            *   Remove entity from old archetype (handle data shifting).
        *   Handle cases where the target archetype doesn't exist (create it).

- [ ] **Story 4.1.5: Implement `registry.get()` and `registry.get_mut()`**
    *   **Description:** Implement the methods for retrieving read-only (`get`) and mutable (`get_mut`) references to a specific component type for a given entity. These methods need to locate the entity within its archetype and then access the correct component column.
    *   **Goal:**
        *   `registry.get::<C: Component>(Entity) -> Option<&C>` is implemented.
        *   `registry.get_mut::<C: Component>(Entity) -> Option<&mut C>` is implemented.
        *   These methods efficiently find the entity's archetype and row, then return a reference to the component data if it exists.
    *   **Tasks:**
        *   Use the entity location map to find the entity's `Archetype` and row index.
        *   Access the correct component column within the `Archetype`.
        *   Perform necessary type casting/downcasting to return `&C` or `&mut C`.
        *   Ensure borrow checking rules are respected, especially for `get_mut`.

- [ ] **Story 4.1.6: Implement `registry.remove::<C>(entity)`**
    *   **Description:** Implement the `registry.remove::<C>()` method to remove a specific component type from an entity. Similar to `insert`, this will involve determining the entity's new archetype (based on its existing components minus the removed one), potentially creating that new archetype, and moving the entity's remaining component data.
    *   **Goal:**
        *   `registry.remove::<C: Component>(Entity)` method is implemented.
        *   The method correctly identifies or creates the target archetype (without component `C`).
        *   Remaining component data for the entity is moved from the old archetype to the new one.
        *   The entity's location is updated.
    *   **Tasks:**
        *   Implement logic to calculate the target `ArchetypeId` when a component is removed.
        *   Implement logic to transition an entity between archetypes (similar to insert, but removing a component type during the copy).
        *   Handle cases where the target archetype doesn't exist (create it).

- [ ] **Story 4.1.7: Unit Tests for Core Registry API with Generic Components**
    *   **Description:** Create a comprehensive suite of unit tests that verify the functionality of all public API methods of the `Registry` (`spawn`, `despawn`, `insert`, `get`, `get_mut`, `remove`). Tests should use various combinations of generic components to ensure correctness and cover edge cases.
    *   **Goal:** High test coverage for the core `Registry` API, ensuring its reliability and correctness under various scenarios (e.g., adding/removing multiple components, entities with no components, entities with many components, despawning and re-spawning entities).
    *   **Tasks:**
        *   Define several simple test component structs (e.g., `Position {x,y}`, `Velocity {dx,dy}`, `TagA`, `TagB`).
        *   Write tests for `spawn` and `despawn`, checking entity ID reuse and generation.
        *   Write tests for `insert` with single and multiple components.
        *   Write tests for `get` and `get_mut`, verifying data integrity and mutability.
        *   Write tests for `remove`, checking component removal and data integrity of remaining components.
        *   Test sequences of operations (e.g., spawn -> insert -> get -> remove -> get -> despawn).
        *   Test edge cases like trying to get/remove non-existent components or operating on despawned entities.

### Epic 4.2: ECS Query Mechanism & Global Resources

- **Description:** Implement a simple query mechanism for iterating over entities with specific component combinations and a system for managing global resources. These should be general-purpose.
- **Goal:** Operations can iterate efficiently over entities matching component queries, and access shared global resources, applicable to any kind of data managed by the ECS.
- **Definition of Done (DoD):**
  - `registry.query::<(&ComponentA, &mut ComponentB)>()` mechanism is implemented, returning an iterator.
  - Query iteration is efficient, leveraging the archetype model.
  - `registry.insert_resource(MyResource)`, `registry.get_resource::<R>()`, `registry.get_resource_mut::<R>()` are implemented.
  - Unit tests cover query functionality with various generic component combinations and resource access.

#### Stories:

- [ ] **Story 4.2.1: Implement Query Iterator Logic for Archetypes**
- [ ] **Story 4.2.2: Implement `registry.query()` API**
- [ ] **Story 4.2.3: Implement Global Resource Storage and API in `Registry`**
- [ ] **Story 4.2.4: Unit Tests for Queries and Resources with Generic Data**

### Epic 4.3: Basic Operation Definition and Runner

- **Description:** Define a pattern for **Operations** (functions operating on `Registry` data via queries and resources) and implement a simple sequential **Operation** runner. **Operations** are envisioned as generic logic units.
- **Goal:** A clear way to define **Operations** exists, and the `Registry` can execute a list of registered **Operations** in order.
- **Definition of Done (DoD):**
  - A convention for **Operation** functions is established (e.g., `fn my_operation(query: Query<...>, res: Res<...>)`).
  - `Registry` can store a list of **Operations**.
  - `registry.run_operations()` (or similar) method executes registered **Operations** sequentially.
  - Basic unit tests demonstrate **Operation** registration and execution with generic test **Operations**.

#### Stories:

- [ ] **Story 4.3.1: Define Operation Function Signature/Trait**
- [ ] **Story 4.3.2: Implement Operation Registration in `Registry`**
- [ ] **Story 4.3.3: Implement Sequential Operation Runner**
- [ ] **Story 4.3.4: Unit Tests for Operation Runner with Generic Operations**

### Epic 4.4: Initial Application: Basic Scene Representation (Demonstrates ECS utility)

- **Description:** As a first *application* of the general-purpose ECS framework (Epics 4.1-4.3), represent a basic scene, for example, the "Persistent Mobile Cube" from Milestone 3. This involves defining specific components like `Position`, `Renderable`, `NetworkSync` and **Operations** to manage them. This demonstrates the ECS's utility for scene management but does not define its sole purpose.
- **Goal:** The ECS framework is actively used to manage the state of at least one type of game object, showcasing its applicability for scene representation as one of many potential uses.
- **Definition of Done (DoD):**
  - Relevant components for a basic scene object (e.g., `Position`, `Velocity`, `MeshID`) are defined *using* the ECS.
  - At least one **Operation** is created *using* the ECS to operate on these components (e.g., a movement **Operation**).
  - The client can spawn and manage a "cube" entity *using* the ECS.
  - (Stretch Goal) Basic synchronization logic with the server for these ECS-managed components is outlined or prototyped.

#### Stories:

- [ ] **Story 4.4.1: Define Core Scene Components (Position, Renderable, etc.) as ECS Components**
- [ ] **Story 4.4.2: Implement a Basic Movement Operation as an ECS Operation**
- [ ] **Story 4.4.3: Integrate ECS for Managing the "Mobile Cube" State (Client-Side)**
## Milestone 5: Engine Systems Foundation

- **Status:** Planned (30-04-2025)
- **Goal:** Establish core systems in the engine-client to enable advanced client-side features such as camera control and user interaction. These systems will leverage the ECS framework where appropriate.

### Epic 5.1: Implement Event System in Engine-Client

- **Description:** Develop a modular event system for the engine-client to facilitate communication between subsystems and support future extensibility. This system may utilize ECS events or integrate with the ECS world.
- **Added:** 30-04-2025

### Epic 5.2: Implement Input System in Engine-Client

- **Description:** Create an input system in the engine-client to process user input events (keyboard, mouse, etc.), enabling interactive features and serving as a foundation for camera and gameplay controls. Input events could be published into the ECS or a dedicated event bus.
- **Added:** 30-04-2025

### Epic 5.3: Implement Filesystem Abstraction Layer

- **Status:** To Plan
- **Description:** Create an abstraction layer for filesystem operations within the engine. This allows using platform-specific efficient methods where available (e.g., OS-specific APIs) while providing a consistent interface. It also paves the way for potential future integration of a Virtual File System (VFS) for accessing packed game assets or other sources.
- **Goal:** A defined trait or set of traits representing filesystem operations (read, write, list, exists, etc.) exists, with an initial implementation potentially using the standard Rust library (`std::fs`).
- **Definition of Done (DoD):**
  - Filesystem abstraction trait(s) defined (e.g., `FileSystemProvider`).
  - Initial implementation using `std::fs` created.
  - The abstraction is integrated into relevant engine parts where file access might be needed later (e.g., configuration loading, initial asset loading - specific integration points can be detailed in stories).
  - Basic unit tests for the abstraction layer exist.

#### Stories:

- [ ] **Story 5.3.1: Define Filesystem Trait(s)**
  - **Description:** Define the core Rust trait(s) (e.g., `FileSystemProvider`) that outline the necessary filesystem operations (read, write, exists, list_directory, etc.).
  - **Goal:** A clear and comprehensive trait definition exists in the engine's core or platform module.

- [ ] **Story 5.3.2: Implement `std::fs` Backend**
  - **Description:** Create the first concrete implementation of the `FileSystemProvider` trait using Rust's standard library `std::fs` module.
  - **Goal:** A functional filesystem provider using `std::fs` is available and passes basic tests.

- [ ] **Story 5.3.3: Basic Integration Point (e.g., Config Loading)**
  - **Description:** Refactor an existing part of the engine that uses file I/O (like configuration loading or a simple asset loader) to use the new `FileSystemProvider` trait instead of directly calling `std::fs`.
  - **Goal:** At least one engine component utilizes the filesystem abstraction, demonstrating its usability.

## Milestone 6: Basic Camera Control

- **Status:** To Plan
- **Goal:** Implement basic camera controls (e.g., orbiting, panning, zooming) on the client-side, allowing the user to change their view of the scene (containing the cube from M3). Camera state and logic can be managed via the ECS.

## Milestone 7: Initial Procedural Noise Generation (Core)

- **Status:** To Plan
- **Goal:** Implement basic procedural noise functions (e.g., Perlin, Simplex) within the `core` engine library, potentially demonstrating their output visually in the client later (e.g., applying noise as texture coordinates or height map).

# Unplanned backlog
- [ ] Server version and protocol version should match between client and server, no checking is made at this moment.
- [ ] Gracefully send a ClientDisconnect message to the server when exiting ?