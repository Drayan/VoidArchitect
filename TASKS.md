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

- **Status:** To Plan
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
- [ ] **Story 2.1.4: Define Basic Render Pass (`VkRenderPass`)**
  - **Description:** Create a `VkRenderPass` object defining a single color attachment (matching the swapchain format) that should be cleared at the start and stored at the end, used in a single subpass.
  - **Goal:** A valid `VkRenderPass` handle suitable for rendering to the swapchain images is created.
- [ ] **Story 2.1.5: Create Framebuffers (`VkFramebuffer`)**
  - **Description:** Create a `VkFramebuffer` for each swapchain image view, linking it to the `VkRenderPass` and specifying the dimensions.
  - **Goal:** A list/vector of `VkFramebuffer` handles, one per swapchain image, is created and stored.

### Epic 2.A: Migrate from Winit to SDL2

- **Status:** To Do (Discovered During Work)
- **Description:** Replace the winit windowing library with SDL2 across all client components. Update the platform abstraction layer, window creation, event loop, and Vulkan surface integration to work with SDL2 instead of winit.
- **Goal:** The client application should function identically but use SDL2 for window management and event handling instead of winit.
- **Definition of Done (DoD):**
  - winit dependency is removed and SDL2 dependency is added to the `engine_client` crate.
  - Platform abstraction layer is refactored to use SDL2 APIs.
  - Window creation code uses SDL2 instead of winit.
  - Event loop implementation is updated to match SDL2's polling model.
  - Vulkan surface creation code uses SDL2's surface creation methods.
  - All functionality from Milestone 1 remains working as before.

#### Stories:

- [ ] **Story 2.A.1: Add SDL2 Dependency**
  - **Description:** Add the SDL2 crate to the `engine_client` project's Cargo.toml and remove winit.
  - **Goal:** SDL2 library is available for use within the client code.
- [ ] **Story 2.A.2: Refactor Platform Abstraction Layer**
  - **Description:** Update the platform module in `engine_client` to use SDL2 instead of winit.
  - **Goal:** The platform layer is fully converted to use SDL2 APIs.
- [ ] **Story 2.A.3: Update Window Creation Code**
  - **Description:** Modify the window initialization code to use SDL2's window creation methods.
  - **Goal:** A visible window is created using SDL2 instead of winit.
- [ ] **Story 2.A.4: Reimplement Event Loop**
  - **Description:** Replace the winit event loop with SDL2's event polling mechanism.
  - **Goal:** The application properly processes window events using SDL2.
- [ ] **Story 2.A.5: Update Vulkan Surface Integration**
  - **Description:** Update the code that creates a Vulkan surface from the window to use SDL2's methods.
  - **Goal:** A valid `VkSurfaceKHR` is created from the SDL2 window.
- [ ] **Story 2.A.6: Test Milestone 1 Functionality**
  - **Description:** Verify that all functionality from Milestone 1 works correctly with SDL2.
  - **Goal:** The application behaves identically to before but now uses SDL2.

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

- [ ] **Story 2.2.1: Write Basic Vertex and Fragment Shaders**
  - **Description:** Create simple GLSL shader files (`.vert`, `.frag`) for a minimal triangle rendering pipeline.
  - **Goal:** GLSL source code for vertex and fragment shaders exists in the `assets` or a dedicated shader directory.
- [ ] **Story 2.2.2: Setup Shader Compilation (SPIR-V)**
  - **Description:** Integrate a shader compiler (like `shaderc-rs`) into the build process or runtime loading to convert GLSL shaders into SPIR-V bytecode.
  - **Goal:** SPIR-V bytecode can be generated from the GLSL source files.
- [ ] **Story 2.2.3: Load SPIR-V into Shader Modules**
  - **Description:** Implement code to read the SPIR-V bytecode and create `VkShaderModule` objects from it.
  - **Goal:** `VkShaderModule` handles for the vertex and fragment shaders are created.
- [ ] **Story 2.2.4: Configure Fixed Pipeline Stages**
  - **Description:** Define the configuration structures for vertex input, input assembly, viewport, scissor, rasterization, multisampling, and color blending states.
  - **Goal:** All necessary configuration structures for the fixed-function stages of the pipeline are populated.
- [ ] **Story 2.2.5: Create Pipeline Layout (`VkPipelineLayout`)**
  - **Description:** Create an empty `VkPipelineLayout` as no external resources (descriptors) or push constants are used for this basic triangle.
  - **Goal:** A valid, empty `VkPipelineLayout` handle is created.
- [ ] **Story 2.2.6: Create Graphics Pipeline (`VkPipeline`)**
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

- [ ] **Story 2.3.1: Define Triangle Vertex Data and Structure**
  - **Description:** Define a Rust struct for vertices (e.g., `struct Vertex { pos: [f32; 2] }`) and create an array/vector containing the data for the three vertices of a triangle.
  - **Goal:** Triangle vertex data exists in a structured format in CPU memory.
- [ ] **Story 2.3.2: Create Vertex Buffer (`VkBuffer`)**
  - **Description:** Implement a function or system to create a `VkBuffer` with `VK_BUFFER_USAGE_VERTEX_BUFFER_BIT` and allocate appropriate `VkDeviceMemory` (likely host-visible for easy upload initially).
  - **Goal:** A `VkBuffer` and associated `VkDeviceMemory` for storing vertex data are created.
- [ ] **Story 2.3.3: Upload Vertex Data to GPU Buffer**
  - **Description:** Map the allocated `VkDeviceMemory`, copy the triangle vertex data from the CPU array/vector into the mapped memory, and unmap it. Handle memory flushing if necessary.
  - **Goal:** The triangle vertex data resides in the GPU buffer.
- [ ] **Story 2.3.4: Create Command Pool (`VkCommandPool`)**
  - **Description:** Create a `VkCommandPool` associated with the graphics queue family index, allowing allocation of command buffers for graphics operations.
  - **Goal:** A valid `VkCommandPool` handle is created.
- [ ] **Story 2.3.5: Allocate Command Buffers (`VkCommandBuffer`)**
  - **Description:** Allocate the required number of command buffers (e.g., one per swapchain image) from the command pool.
  - **Goal:** `VkCommandBuffer` handles are allocated and ready for recording.
- [ ] **Story 2.3.6: Record Draw Commands**
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

- [ ] **Story 2.4.1: Create Synchronization Primitives**
  - **Description:** Create the necessary `VkSemaphore` and `VkFence` objects required for synchronizing swapchain operations, command buffer submission, and presentation. Manage multiple sets if using multiple frames in flight.
  - **Goal:** Valid handles for required semaphores and fences are created.
- [ ] **Story 2.4.2: Implement Frame Acquisition (`vkAcquireNextImageKHR`)**
  - **Description:** Call `vkAcquireNextImageKHR` within the render loop to get the next available swapchain image index and signal the appropriate semaphore.
  - **Goal:** The index of the next image to render to is obtained.
- [ ] **Story 2.4.3: Implement Command Buffer Submission (`vkQueueSubmit`)**
  - **Description:** Call `vkQueueSubmit` with the correct command buffer, wait/signal semaphores, and the completion fence for the current frame in flight.
  - **Goal:** The pre-recorded command buffer is submitted to the graphics queue for execution.
- [ ] **Story 2.4.4: Implement Presentation (`vkQueuePresentKHR`)**
  - **Description:** Call `vkQueuePresentKHR` to queue the rendered image for presentation to the screen, waiting on the render finished semaphore.
  - **Goal:** The completed frame is submitted for display on the window.
- [ ] **Story 2.4.5: Integrate Rendering into Client Event Loop**
  - **Description:** Modify the `client`'s main event loop (`winit`) to continuously call the rendering functions (acquire, submit, present) during `MainEventsCleared` or `RedrawRequested` events.
  - **Goal:** Rendering happens repeatedly, displaying the triangle continuously.
- [ ] **Story 2.4.6: Add Basic Swapchain Resize Handling**
  - **Description:** Detect `VK_ERROR_OUT_OF_DATE_KHR` or `VK_SUBOPTIMAL_KHR` results from acquire/present calls and trigger a (potentially placeholder) swapchain recreation process. Ensure CPU waits for GPU idle before recreating.
  - **Goal:** The application can gracefully handle (even if just by pausing rendering) window resize events that invalidate the swapchain.

## Milestone 3: Persistent Mobile Cube

- **Status:** To Plan
- **Goal:** Display a 3D cube in the client window that can be moved based on state information received from the server. The cube's state is maintained on the server and synchronized over the network.

### Epic 3.1: Server-Side Object State Management

- **Description:** Define and manage the state (position, rotation) of a simple game object (a cube) on the server. Implement logic for the server to hold and potentially update this state.
- **Goal:** The server has an authoritative representation of the cube's transform in its memory.
- **Definition of Done (DoD):**
  - A data structure representing the cube's state (e.g., `struct CubeState { position: Vec3, rotation: Quat }`) is defined in the `void-architect_shared` crate.
  - The `server` application maintains at least one instance of this `CubeState`.
  - A basic mechanism exists within the server to modify this state (e.g., a simple periodic update in a `tokio` task, or placeholder for future input).
  - State management code resides in the `server` crate (or `engine_server` if abstracting).

#### Stories:

- [ ] **Story 3.1.1: Define Shared Cube State Structure**
  - **Description:** Add the `CubeState` struct (including necessary math types like vectors/quaternions, perhaps from `glam` or `nalgebra`) to the `void-architect_shared` crate.
  - **Goal:** A common definition for the cube's transform exists and is accessible by both client and server.
- [ ] **Story 3.1.2: Implement Server-Side State Storage**
  - **Description:** In the `server` crate, create and manage the lifecycle of the `CubeState` data (e.g., within the server's main state or associated with a client connection).
  - **Goal:** The server holds the authoritative `CubeState` in its memory.
- [ ] **Story 3.1.3: Implement Basic Server-Side State Update Logic**
  - **Description:** Add simple logic to the server to modify the `CubeState` over time (e.g., slowly rotate the cube or move it along an axis on a timer).
  - **Goal:** The cube's state on the server changes dynamically.

### Epic 3.2: Network State Synchronization

- **Description:** Define Protobuf messages for sending the cube's state. Implement server logic to send updates to connected clients and client logic to receive and process these updates.
- **Goal:** The client application receives timely updates of the cube's state from the server via the network.
- **Definition of Done (DoD):**
  - A Protobuf message (e.g., `message CubeStateUpdate { Vector3 position; Quaternion rotation; }`) is defined in `shared/proto`.
  - The `void-architect_shared` crate is updated with the generated Rust code for this message.
  - The server periodically (or on change) serializes its `CubeState` into the Protobuf message and sends it over the established TCP connection to the client.
  - The client's network task receives the `CubeStateUpdate` message, deserializes it, and updates its local copy of the cube's state.
  - Network communication logic is updated in `server` and `client` (potentially abstracted into `engine_server`/`engine_client`).

#### Stories:

- [ ] **Story 3.2.1: Define Cube State Protobuf Message**
  - **Description:** Create the `.proto` definition for the `CubeStateUpdate` message, including necessary nested types for vectors/quaternions if not using primitive types. Regenerate `shared` crate code.
  - **Goal:** Protobuf definition and corresponding Rust code for state synchronization exist.
- [ ] **Story 3.2.2: Implement Server Logic to Send State Updates**
  - **Description:** Modify the server's network logic to periodically read the current `CubeState`, serialize it using `prost`, and send it to the connected client(s).
  - **Goal:** The server transmits cube state updates over the network.
- [ ] **Story 3.2.3: Implement Client Logic to Receive State Updates**
  - **Description:** Modify the client's network logic to listen for `CubeStateUpdate` messages, deserialize them using `prost`, and store the received state (e.g., in an `Arc<Mutex<...>>` or channel accessible by the render thread).
  - **Goal:** The client receives and stores the cube state updates from the network.

### Epic 3.3: Client-Side 3D Rendering with Transforms

- **Description:** Update the client's rendering pipeline to draw a 3D cube instead of a 2D triangle. Use the synchronized state received from the server to calculate and apply model transformations (position, rotation) to the cube before rendering.
- **Goal:** The client renders a 3D cube whose position and orientation in the scene match the state received from the server.
- **Definition of Done (DoD):**
  - Vertex and potentially index data for a 3D cube mesh are defined.
  - GPU buffers (vertex, index) are created and populated with the cube mesh data.
  - Vertex shader is updated to accept 3D positions and apply a Model-View-Projection (MVP) transformation matrix.
  - A mechanism to provide the MVP matrix to the vertex shader is implemented (e.g., Uniform Buffer Object - UBO, or Push Constants).
  - The client's rendering logic reads the latest received `CubeState`.
  - Model matrix is calculated based on the cube's position/rotation. View and Projection matrices are set up (simple static camera initially). MVP matrix is calculated.
  - The MVP matrix is updated in the UBO or pushed via push constants before the draw call.
  - Command buffers are updated to bind the cube's vertex/index buffers and use the new pipeline/shaders.
  - A 3D cube is visibly rendered in the client window, and its position/rotation updates according to the data received from the server.
  - Rendering updates reside primarily in `engine_client`, state reading/matrix calculation might be in `client` or `engine_client`.

#### Stories:

- [ ] **Story 3.3.1: Define Cube Mesh Data (Vertices, Indices)**
  - **Description:** Define the vertex positions (and potentially colors or normals) and index order for rendering a 3D cube.
  - **Goal:** Data arrays for the cube's vertices and indices exist in CPU memory.
- [ ] **Story 3.3.2: Create/Update GPU Buffers for Cube**
  - **Description:** Create or update the `VkBuffer`s and associated memory for storing the cube's vertex and index data on the GPU. Upload the data.
  - **Goal:** Cube mesh data resides in GPU buffers ready for rendering.
- [ ] **Story 3.3.3: Update Shaders for 3D and MVP Transforms**
  - **Description:** Modify the vertex shader to take 3D vertex positions and an MVP matrix (e.g., via UBO or push constant) and calculate `gl_Position`. Modify fragment shader if needed (e.g., basic lighting based on normals, or just flat color). Compile shaders to SPIR-V.
  - **Goal:** Shaders capable of rendering a transformed 3D mesh exist.
- [ ] **Story 3.3.4: Implement MVP Matrix Uniform (UBO or Push Constant)**
  - **Description:** Choose a method (UBO preferred for matrices, Push Constant for smaller/frequent data) to pass the MVP matrix to the vertex shader. Implement the necessary Vulkan setup (Descriptor Set Layout, Descriptor Pool/Set, UBO buffer creation/update OR Pipeline Layout update for Push Constants).
  - **Goal:** A mechanism exists to send the MVP matrix data to the GPU for use by the vertex shader.
- [ ] **Story 3.3.5: Update Graphics Pipeline for 3D Cube**
  - **Description:** Recreate or update the `VkPipeline` and potentially `VkPipelineLayout` to use the new 3D shaders, updated vertex input definitions (3D position), and the MVP uniform mechanism. Enable depth testing.
  - **Goal:** A graphics pipeline configured for rendering the transformed 3D cube exists.
- [ ] **Story 3.3.6: Update Render Loop to Apply Networked State**
  - **Description:** In the client's render loop, read the latest `CubeState` received from the network thread. Calculate the Model, View (static camera for now), and Projection matrices. Compute the final MVP matrix. Update the UBO or set the push constant data with the new MVP matrix before recording/submitting the draw commands. Update draw commands for indexed drawing (`vkCmdDrawIndexed`).
  - **Goal:** The cube's transform in the rendered scene reflects the state synchronized from the server.

## Milestone 4: Basic Camera Control

- **Status:** To Plan
- **Goal:** Implement basic camera controls (e.g., orbiting, panning, zooming) on the client-side, allowing the user to change their view of the scene (containing the cube from M3).

## Milestone 5: Basic Scene Representation

- **Status:** To Plan
- **Goal:** Develop a rudimentary scene graph or entity management system on the client and server to handle multiple objects, not just a single cube. Synchronize the state of multiple objects.

## Milestone 6: Initial Procedural Noise Generation (Core)

- **Status:** To Plan
- **Goal:** Implement basic procedural noise functions (e.g., Perlin, Simplex) within the `core` engine library, potentially demonstrating their output visually in the client later (e.g., applying noise as texture coordinates or height map).
