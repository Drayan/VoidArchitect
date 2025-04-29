use crate::renderer::backends::vulkan::swapchain::VulkanSwapchainContext;

use super::VulkanFramebuffer;
use super::commands::VulkanCommandBuffer;
use super::device::VulkanDevice;
use super::fence::VulkanFence;
use super::renderpass::VulkanRenderPass;
use super::swapchain::VulkanSwapchain;
use ash::vk;
use sdl3::sys::vulkan;

// VulkanContext definition and implementation moved from mod.rs
pub(super) struct VulkanContext {
    // Current framebuffer width and height.
    pub framebuffer_width: u32,
    pub framebuffer_height: u32,
    pub framebuffer_size_generation: u64,
    pub framebuffer_size_last_generation: u64,

    // Vulkan instance and surface (None if not initialized).
    pub instance: Option<ash::Instance>,
    pub surface: Option<ash::vk::SurfaceKHR>,

    // Vulkan loader handles for surface and swapchain extensions.
    pub surface_loader: Option<ash::khr::surface::Instance>,
    pub swapchain_loader: Option<ash::khr::swapchain::Device>,

    // Debug utilities (only in debug builds).
    #[cfg(debug_assertions)]
    pub debug_callback: Option<ash::vk::DebugUtilsMessengerEXT>,
    #[cfg(debug_assertions)]
    pub debug_utils_loader: Option<ash::ext::debug_utils::Instance>,

    // Logical device and swapchain state.
    pub device: Option<VulkanDevice>,
    pub swapchain: Option<VulkanSwapchain>,
    pub main_renderpass: Option<VulkanRenderPass>,

    // Command buffers for graphics operations.
    pub graphics_cmds_buffers: Vec<VulkanCommandBuffer>,

    pub image_available_semaphores: Vec<ash::vk::Semaphore>,
    pub queue_complete_semaphores: Vec<ash::vk::Semaphore>,

    pub in_flight_fences: Vec<VulkanFence>,
    pub images_in_flight: Vec<Option<usize>>,

    // Current image index and frame number.
    pub image_index: usize,
    pub current_frame: usize,

    // Flag indicating if swapchain is being recreated.
    pub recreating_swapchain: bool,
}

impl VulkanContext {
    pub fn new() -> Self {
        VulkanContext {
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
        }
    }
    /// Finds a suitable memory type for the given memory requirements and flags.
    ///
    /// # Arguments
    /// * `type_filter` - The memory type filter.
    /// * `properties` - The desired memory properties.
    ///
    /// # Returns
    /// * `u32` - The index of the suitable memory type.
    pub fn find_memory_type(
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

impl VulkanContext {
    pub fn wait_for_device_idle(&self) -> Result<(), String> {
        // Wait for the device to be idle.
        let device = self
            .device
            .as_ref()
            .and_then(|d| d.logical_device.as_ref())
            .ok_or("Failed to get Vulkan device")?;

        match unsafe { device.device_wait_idle() } {
            Ok(_) => {}
            Err(err) => return Err(format!("Failed to wait for device idle: {:?}", err)),
        }

        Ok(())
    }
    pub fn create_commands_buffers(&mut self) -> Result<(), String> {
        // If the context already has command buffers, destroy them
        // Extract needed values first to avoid multiple borrows
        let logical_device = self.device.as_ref().unwrap().logical_device.as_ref().unwrap();
        let command_pool = self.device.as_ref().unwrap().graphics_command_pool;

        for buffer in self.graphics_cmds_buffers.iter_mut() {
            buffer.destroy(logical_device, command_pool);
        }
        self.graphics_cmds_buffers.clear();

        // Create new command buffers
        // Extract all needed values first
        let swapchain = self.swapchain.as_ref().unwrap();
        let device = self.device.as_ref().unwrap();
        let logical_device = device.logical_device.as_ref().unwrap();
        let command_pool = device.graphics_command_pool;
        let image_count = swapchain.images.len();

        // Now create each buffer with the extracted values
        for _ in 0..image_count {
            let buffer = VulkanCommandBuffer::new(logical_device, command_pool, true)?;
            self.graphics_cmds_buffers.push(buffer);
        }

        Ok(())
    }

    pub fn destroy_commands_buffers(&mut self) -> Result<(), String> {
        // Destroy Vulkan command buffers here
        // Extract needed values first to avoid multiple borrows
        let logical_device = self.device.as_ref().unwrap().logical_device.as_ref().unwrap();
        let command_pool = self.device.as_ref().unwrap().graphics_command_pool;

        // Now destroy each buffer with the extracted values
        for buffer in self.graphics_cmds_buffers.iter_mut() {
            buffer.destroy(logical_device, command_pool);
        }
        self.graphics_cmds_buffers.clear();
        Ok(())
    }

    pub fn recreate_framebuffers(&mut self) -> Result<(), String> {
        let swapchain = self.swapchain.as_mut().ok_or("Failed to get swapchain")?;
        let device = self
            .device
            .as_ref()
            .and_then(|d| d.logical_device.as_ref())
            .ok_or("Failed to get Vulkan device")?;
        let renderpass =
            self.main_renderpass.as_ref().ok_or("Failed to get main renderpass")?;
        let width = self.framebuffer_width;
        let height = self.framebuffer_height;

        swapchain.image_views.iter().for_each(|view| {
            let attachments = vec![view.clone(), swapchain.depth_attachment.view.clone()];
            let framebuffer = match VulkanFramebuffer::new(
                device,
                width,
                height,
                renderpass.clone(),
                attachments,
            ) {
                Ok(framebuffer) => framebuffer,
                Err(e) => {
                    log::error!("Failed to create Vulkan framebuffer: {:?}", e);
                    return;
                }
            };
            swapchain.framebuffers.push(framebuffer);
        });
        Ok(())
    }

    pub fn resize_swapchain_if_needed(&mut self) -> Result<bool, String> {
        // Check if recreating the swapchain is in progress and skip the frame is so.
        if self.recreating_swapchain {
            // Wait for the device to be idle before resizing.
            let device = self
                .device
                .as_ref()
                .and_then(|d| d.logical_device.as_ref())
                .ok_or("Failed to get Vulkan device")?;

            match unsafe { device.device_wait_idle() } {
                Ok(_) => {}
                Err(err) => return Err(format!("Failed to wait for device idle: {:?}", err)),
            }

            log::debug!("Recreating swapchain, skipping frame");
            return Ok(false); // Skip the frame.
        }

        // Check if the framebuffer size has changed.
        if self.framebuffer_size_generation != self.framebuffer_size_last_generation {
            let width = self.framebuffer_width;
            let height = self.framebuffer_height;
            log::debug!("Framebuffer size changed: {}x{}", width, height);

            // Wait for the device to be idle before resizing.
            let device = self
                .device
                .as_ref()
                .and_then(|d| d.logical_device.as_ref())
                .ok_or("Failed to get Vulkan device")?;
            match unsafe { device.device_wait_idle() } {
                Ok(_) => {}
                Err(err) => return Err(format!("Failed to wait for device idle: {:?}", err)),
            }

            // Recreate the swapchain.
            match self.swapchain().recreate(width, height) {
                Ok(_) => {
                    log::debug!("Swapchain recreated: {}x{}", width, height);
                    return Ok(false); // Skip the frame.
                }
                Err(err) => {
                    return Err(format!(
                        "Failed to recreate swapchain: {}x{}: {:?}",
                        width, height, err
                    ));
                }
            }
        }

        Ok(true) // Continue with the frame.
    }

    pub fn acquire_next_image(&mut self) -> Result<bool, String> {
        // Wait for the execution of the current frame to finish.
        match self.wait_for_image_fence(self.current_frame) {
            Ok(_) => {}
            Err(err) => return Err(format!("Failed to wait for image fence: {:?}", err)),
        }

        let width = self.framebuffer_width;
        let height = self.framebuffer_height;
        let semaphore = self.image_available_semaphores[self.current_frame];
        match self.swapchain().acquire_next_image_index(
            width,
            height,
            u64::MAX,
            &semaphore,
            None,
        ) {
            Ok(index) => {
                self.image_index = index;
            }
            Err(e) => {
                log::warn!("Failed to acquire image index: {:?}", e);
                return Ok(false); // Skip the frame.
            }
        }

        Ok(true) // Continue with the frame.
    }

    pub fn begin_gfx_commands_buffer(&mut self) -> Result<(), String> {
        let command_buffer = self
            .graphics_cmds_buffers
            .get_mut(self.image_index)
            .ok_or("Failed to get command buffer")?;
        let device = self
            .device
            .as_ref()
            .and_then(|d| d.logical_device.as_ref())
            .ok_or("Failed to get Vulkan device")?;

        command_buffer.reset();
        command_buffer.begin(device, false, false, false)
    }

    pub fn begin_main_render_pass(&mut self) -> Result<(), String> {
        let width = self.framebuffer_width;
        let height = self.framebuffer_height;
        let framebuffer = self
            .swapchain
            .as_ref()
            .and_then(|s| s.framebuffers.get(self.image_index))
            .ok_or("Failed to get framebuffer")?;
        let device = self
            .device
            .as_ref()
            .and_then(|d| d.logical_device.as_ref())
            .ok_or("Failed to get Vulkan device")?;
        let command_buffer = self
            .graphics_cmds_buffers
            .get_mut(self.image_index)
            .ok_or("Failed to get command buffer")?;

        // Set viewport and scissor
        let viewport = vk::Viewport {
            x: 0.0,
            y: height as f32,
            width: width as f32,
            height: -(height as f32),
            min_depth: 0.0,
            max_depth: 1.0,
        };

        let scissor = vk::Rect2D {
            offset: vk::Offset2D { x: 0, y: 0 },
            extent: vk::Extent2D { width, height },
        };

        let renderpass =
            self.main_renderpass.as_mut().ok_or("Failed to get main renderpass")?;
        renderpass.w = self.framebuffer_width as f32;
        renderpass.h = self.framebuffer_height as f32;

        command_buffer.set_viewports(device, [viewport].to_vec())?;
        command_buffer.set_scissors(device, [scissor].to_vec())?;

        renderpass.begin(device, command_buffer, framebuffer.handle);

        Ok(())
    }

    pub fn end_main_render_pass(&mut self) -> Result<(), String> {
        let cmdsbuf = self
            .graphics_cmds_buffers
            .get_mut(self.image_index)
            .ok_or("Failed to get command buffer")?;

        let device = self
            .device
            .as_ref()
            .and_then(|d| d.logical_device.as_ref())
            .ok_or("Failed to get Vulkan device")?;

        let renderpass =
            self.main_renderpass.as_mut().ok_or("Failed to get main renderpass")?;

        renderpass.end(device, cmdsbuf);
        Ok(())
    }

    pub fn end_gfx_commands_buffer(&mut self) -> Result<(), String> {
        let cmdsbuf = self
            .graphics_cmds_buffers
            .get_mut(self.image_index)
            .ok_or("Failed to get command buffer")?;

        let device = self
            .device
            .as_ref()
            .and_then(|d| d.logical_device.as_ref())
            .ok_or("Failed to get Vulkan device")?;

        cmdsbuf.end(device)?;
        Ok(())
    }

    pub fn wait_for_previous_frame_to_complete(&mut self) -> Result<(), String> {
        if self.images_in_flight[self.image_index].is_some() {
            self.wait_for_image_fence(self.image_index)?;
        }

        Ok(())
    }

    pub fn wait_for_image_fence(&mut self, fence_index: usize) -> Result<(), String> {
        let fence = self
            .in_flight_fences
            .get_mut(fence_index)
            .ok_or("Failed to get in-flight fence")?;

        let device = self
            .device
            .as_ref()
            .and_then(|d| d.logical_device.as_ref())
            .ok_or("Failed to get Vulkan device")?;

        fence.wait(device, u64::MAX)?;
        Ok(())
    }

    pub fn update_image_sync(&mut self) -> Result<(), String> {
        let current_fence_index = self.current_frame;
        let image_index = self.image_index;

        self.images_in_flight[image_index] = Some(current_fence_index);

        // Reset the fence for the *current* frame, which will be used next time this frame index comes up.
        let device = self
            .device
            .as_ref()
            .and_then(|d| d.logical_device.as_ref())
            .ok_or("Failed to get Vulkan device")?;

        self.in_flight_fences[current_fence_index].reset(device)?;
        Ok(())
    }

    pub fn submit_gfx_command_buf(&mut self) -> Result<(), String> {
        let device = self
            .device
            .as_ref()
            .and_then(|d| d.logical_device.as_ref())
            .ok_or("Failed to get Vulkan device")?;

        let cmdsbuf = self
            .graphics_cmds_buffers
            .get(self.image_index)
            .ok_or("Failed to get command buffer")?;

        let queue = self
            .device
            .as_ref()
            .and_then(|d| Some(d.graphics_queue))
            .ok_or("Failed to get graphics queue")?;

        let fence = self
            .in_flight_fences
            .get(self.current_frame)
            .ok_or("Failed to get in-flight fence")?;

        let commands_buffers = [cmdsbuf.handle];
        let wait_semaphores = [self.image_available_semaphores[self.current_frame]];
        let signal_semaphores = [self.queue_complete_semaphores[self.current_frame]];
        let pipeline_stage_flags = [vk::PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT];

        let submit_info = vk::SubmitInfo::default()
            .command_buffers(&commands_buffers)
            .wait_semaphores(&wait_semaphores)
            .signal_semaphores(&signal_semaphores)
            .wait_dst_stage_mask(&pipeline_stage_flags);

        match unsafe { device.queue_submit(queue, &[submit_info], fence.handle()) } {
            Ok(_) => {}
            Err(err) => return Err(format!("Failed to submit command buffer: {:?}", err)),
        }

        let cmdsbuf = self
            .graphics_cmds_buffers
            .get_mut(self.image_index)
            .ok_or("Failed to get command buffer")?;
        cmdsbuf.update_submitted();

        Ok(())
    }

    pub fn present(&mut self) -> Result<(), String> {
        let width = self.framebuffer_width;
        let height = self.framebuffer_height;
        let present_queue = self
            .device
            .as_ref()
            .and_then(|d| Some(d.present_queue))
            .ok_or("Failed to get present queue")?;
        let render_completed_semaphore = self
            .queue_complete_semaphores
            .get(self.current_frame)
            .ok_or("Failed to get render completed semaphore")?
            .clone();
        let image_index = self.image_index;

        self.swapchain().present(
            width,
            height,
            present_queue,
            render_completed_semaphore,
            image_index,
        );

        self.current_frame = (self.current_frame + 1)
            % self.swapchain.as_ref().ok_or("Failed to get swapchain")?.max_frames_in_flight;
        Ok(())
    }

    pub fn recreate_swapchain(&mut self, width: u32, height: u32) -> Result<(), String> {
        // If already recreating, skip
        if self.recreating_swapchain {
            return Ok(());
        }

        // Detect if the window is too small to create a swapchain
        if self.framebuffer_width == 0 || self.framebuffer_height == 0 {
            return Ok(());
        }

        // Set the recreating flag to true
        self.recreating_swapchain = true;

        // Wait for the device to be idle before resizing.
        self.wait_for_device_idle()?;

        // Clear images just in case
        for image in self.images_in_flight.iter_mut() {
            *image = None;
        }

        // Requery swapchain support details
        {
            let phys_device = self
                .device
                .as_ref()
                .and_then(|d| Some(d.physical_device))
                .ok_or("Failed to get Vulkan physical device")?;
            let surface = self.surface.as_ref().ok_or("Failed to get Vulkan surface")?;
            let surface_loader =
                self.surface_loader.as_ref().ok_or("Failed to get Vulkan surface loader")?;
            let swapchain_support =
                VulkanDevice::query_swapchain_support(surface_loader, phys_device, *surface)?;

            let vulkan_device = self.device.as_mut().ok_or("Failed to get Vulkan device")?;
            vulkan_device.swapchain_support = swapchain_support;

            let instance = self.instance.as_ref().ok_or("Failed to get Vulkan instance")?;
            vulkan_device.detect_depth_format(instance)?;

            self.swapchain().recreate(width, height)?;
        }

        {
            let renderpass =
                self.main_renderpass.as_mut().ok_or("Failed to get main renderpass")?;
            self.framebuffer_width = width;
            self.framebuffer_height = height;
            renderpass.w = width as f32;
            renderpass.h = height as f32;
            renderpass.x = 0.0;
            renderpass.y = 0.0;

            self.framebuffer_size_last_generation = self.framebuffer_size_generation;
        }

        // Cleanup
        {
            let vulkan_device = self.device.as_mut().ok_or("Failed to get Vulkan device")?;
            let device =
                vulkan_device.logical_device.as_ref().ok_or("Failed to get Vulkan device")?;
            for buf in self.graphics_cmds_buffers.iter_mut() {
                buf.destroy(device, vulkan_device.graphics_command_pool);
            }
        }

        {
            let device = self
                .device
                .as_ref()
                .and_then(|d| d.logical_device.as_ref())
                .ok_or("Failed to get Vulkan device")?;
            let swapchain = self.swapchain.as_mut().ok_or("Failed to get swapchain")?;
            for buf in swapchain.framebuffers.iter_mut() {
                buf.destroy(device);
            }

            self.recreate_framebuffers()?;
        }

        self.create_commands_buffers()?;

        // Reset the recreating flag
        self.recreating_swapchain = false;

        Ok(())
    }
}
