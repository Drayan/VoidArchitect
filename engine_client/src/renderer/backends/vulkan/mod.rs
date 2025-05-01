//! Vulkan backend implementation for the renderer.
//!
//! This module provides the `RendererBackendVulkan` struct, which implements the `RendererBackend`
//! trait using the Vulkan API via the `ash` crate. It manages Vulkan instance, device, surface,
//! and related resources, and provides methods for initialization, frame rendering, resizing,
//! and shutdown. Debug utilities are enabled in debug builds.

use framebuffer::VulkanFramebuffer;

mod backend;
mod commands;
mod device;
mod fence;
mod framebuffer;
mod image;
mod renderpass;
mod swapchain;

use ash::vk;
use commands::VulkanCommandBuffer;
use device::VulkanDevice;
use fence::VulkanFence;
use renderpass::VulkanRenderPass;
use swapchain::VulkanSwapchain;

pub struct VulkanRendererBackend {
    // Vulkan context
    // Holds the Vulkan context state for this backend.

    // Current framebuffer width and height.
    framebuffer_width: u32,
    framebuffer_height: u32,
    framebuffer_size_generation: u64,
    framebuffer_size_last_generation: u64,

    // Vulkan instance and surface (None if not initialized).
    instance: Option<ash::Instance>,
    surface: Option<vk::SurfaceKHR>,

    // Vulkan loader handles for surface and swapchain extensions.
    surface_loader: Option<ash::khr::surface::Instance>,
    swapchain_loader: Option<ash::khr::swapchain::Device>,

    // Debug utilities (only in debug builds).
    #[cfg(debug_assertions)]
    debug_callback: Option<vk::DebugUtilsMessengerEXT>,
    #[cfg(debug_assertions)]
    debug_utils_loader: Option<ash::ext::debug_utils::Instance>,

    // Logical device and swapchain state.
    device: Option<VulkanDevice>,
    swapchain: Option<VulkanSwapchain>,
    main_renderpass: Option<VulkanRenderPass>,

    // Command buffers for graphics operations.
    graphics_cmds_buffers: Vec<VulkanCommandBuffer>,

    image_available_semaphores: Vec<vk::Semaphore>,
    queue_complete_semaphores: Vec<vk::Semaphore>,

    in_flight_fences: Vec<VulkanFence>,
    images_in_flight: Vec<Option<usize>>,

    // Current image index and frame number.
    image_index: usize,
    current_frame: usize,

    // Flag indicating if swapchain is being recreated.
    recreating_swapchain: bool,

    cached_framebuffer_width: u32,
    cached_framebuffer_height: u32,
}

impl VulkanRendererBackend {
    /// Creates a new Vulkan renderer backend instance.
    ///
    /// # Returns
    /// * `RendererBackendVulkan` - A new Vulkan backend instance.
    pub fn new() -> Self {
        Self {
            framebuffer_width: 0,
            framebuffer_height: 0,
            framebuffer_size_generation: 0,
            framebuffer_size_last_generation: 0,

            instance: None,
            surface: None,
            surface_loader: None,
            swapchain_loader: None,

            #[cfg(debug_assertions)]
            debug_callback: None,
            #[cfg(debug_assertions)]
            debug_utils_loader: None,

            device: None,
            swapchain: None,
            main_renderpass: None,
            graphics_cmds_buffers: Vec::new(),

            image_available_semaphores: Vec::new(),
            queue_complete_semaphores: Vec::new(),
            in_flight_fences: Vec::new(),
            images_in_flight: Vec::new(),

            image_index: 0,
            current_frame: 0,
            recreating_swapchain: false,

            cached_framebuffer_width: 0,
            cached_framebuffer_height: 0,
        }
    }
}

impl VulkanRendererBackend {
    /// Finds a suitable memory type for the given memory requirements and flags.
    ///
    /// # Arguments
    /// * `type_filter` - The memory type filter.
    /// * `properties` - The desired memory properties.
    ///
    /// # Returns
    /// * `u32` - The index of the suitable memory type.
    fn find_memory_type(
        physical_device: ash::vk::PhysicalDevice,
        instance: &ash::Instance,
        type_filter: u32,
        properties: vk::MemoryPropertyFlags,
    ) -> Result<u32, String> {
        // # Reason: Vulkan requires selecting a memory type that matches both a bitmask (type_filter)
        // and required property flags. This iterates all memory types and returns the first match.
        let mem_properties =
            unsafe { instance.get_physical_device_memory_properties(physical_device) };

        for i in 0..mem_properties.memory_type_count {
            // Check if this memory type is allowed by the filter and has the required properties.
            if (type_filter & (1 << i)) != 0
                && mem_properties.memory_types[i as usize].property_flags.contains(properties)
            {
                return Ok(i);
            }
        }

        // No suitable memory type found.
        Err(format!(
            "Failed to find suitable memory type for filter: {} and properties: {:?}",
            type_filter, properties
        ))
    }
}

// --- VulkanRendererBackend internal accessors macros ---
#[macro_export]
macro_rules! device {
    ($self:expr) => {
        $self
            .device
            .as_ref()
            .and_then(|d| d.logical_device.as_ref())
            .ok_or("Failed to get Vulkan device")?
    };
}

#[macro_export]
macro_rules! vulkan_device {
    ($self:expr) => {
        $self.device.as_ref().ok_or("Failed to get Vulkan device")?
    };
}

#[macro_export]
macro_rules! vulkan_device_mut {
    ($self:expr) => {
        $self.device.as_mut().ok_or("Failed to get Vulkan device")?
    };
}

#[macro_export]
macro_rules! instance {
    ($self:expr) => {
        $self.instance.as_ref().ok_or("Failed to get Vulkan instance")?
    };
}

#[macro_export]
macro_rules! instace_mut {
    ($self:expr) => {
        $self.instance.as_mut().ok_or("Failed to get Vulkan instance")?
    };
}

#[macro_export]
macro_rules! surface {
    ($self:expr) => {
        $self.surface.as_ref().ok_or("Failed to get Vulkan surface")?
    };
}

#[macro_export]
macro_rules! surface_mut {
    ($self:expr) => {
        $self.surface.as_mut().ok_or("Failed to get Vulkan surface")?
    };
}

#[macro_export]
macro_rules! surface_loader {
    ($self:expr) => {
        $self.surface_loader.as_ref().ok_or("Failed to get Vulkan surface loader")?
    };
}

#[macro_export]
macro_rules! surface_loader_mut {
    ($self:expr) => {
        $self.surface_loader.as_mut().ok_or("Failed to get Vulkan surface loader")?
    };
}

#[macro_export]
macro_rules! swapchain_loader {
    ($self:expr) => {
        $self.swapchain_loader.as_ref().ok_or("Failed to get Vulkan swapchain loader")?
    };
}

#[macro_export]
macro_rules! swapchain_loader_mut {
    ($self:expr) => {
        $self.swapchain_loader.as_mut().ok_or("Failed to get Vulkan swapchain loader")?
    };
}

#[macro_export]
macro_rules! swapchain {
    ($self:expr) => {
        $self.swapchain.as_ref().ok_or("Failed to get Vulkan swapchain")?
    };
}

#[macro_export]
macro_rules! swapchain_mut {
    ($self:expr) => {
        $self.swapchain.as_mut().ok_or("Failed to get Vulkan swapchain")?
    };
}

#[macro_export]
macro_rules! main_renderpass {
    ($self:expr) => {
        $self.main_renderpass.as_ref().ok_or("Failed to get main renderpass")?
    };
}

#[macro_export]
macro_rules! main_renderpass_mut {
    ($self:expr) => {
        $self.main_renderpass.as_mut().ok_or("Failed to get main renderpass")?
    };
}

#[macro_export]
macro_rules! command_buffer {
    ($self:expr, $index:expr) => {
        $self.graphics_cmds_buffers.get($index).ok_or("Failed to get command buffer")?
    };
}

#[macro_export]
macro_rules! command_buffer_mut {
    ($self:expr, $index:expr) => {
        $self.graphics_cmds_buffers.get_mut($index).ok_or("Failed to get command buffer")?
    };
}

#[macro_export]
macro_rules! framebuffer {
    ($self:expr, $index:expr) => {
        $self
            .swapchain
            .as_ref()
            .and_then(|s| s.framebuffers.get($index))
            .ok_or("Failed to get framebuffer")?
    };
}

#[macro_export]
macro_rules! framebuffer_mut {
    ($self:expr, $index:expr) => {
        $self
            .swapchain
            .as_mut()
            .and_then(|s| s.framebuffers.get_mut($index))
            .ok_or("Failed to get framebuffer")?
    };
}

#[macro_export]
macro_rules! fence {
    ($self:expr, $index:expr) => {
        $self.in_flight_fences.get($index).ok_or("Failed to get in-flight fence")?
    };
}

#[macro_export]
macro_rules! fence_mut {
    ($self:expr, $index:expr) => {
        $self.in_flight_fences.get_mut($index).ok_or("Failed to get in-flight fence")?
    };
}
