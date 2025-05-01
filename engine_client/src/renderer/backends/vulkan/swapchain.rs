use ash::vk;

use crate::{
    device, instance, main_renderpass_mut, surface, surface_loader, swapchain,
    swapchain_loader, swapchain_mut, vulkan_device, vulkan_device_mut,
};

use super::{
    VulkanFramebuffer, VulkanRendererBackend,
    commands::VulkanCommandBufferContext,
    device::{VulkanDevice, VulkanDeviceContext},
    fence::VulkanFenceContext,
    framebuffer::VulkanFramebufferContext,
    image::VulkanImage,
};

pub(super) struct VulkanSwapchain {
    pub surface_format: vk::SurfaceFormatKHR,
    pub max_frames_in_flight: usize,
    pub handle: vk::SwapchainKHR,
    pub images: Vec<vk::Image>,
    pub image_views: Vec<vk::ImageView>,
    pub depth_attachment: VulkanImage,
    pub framebuffers: Vec<VulkanFramebuffer>,
}

pub(super) struct VulkanSwapchainOperations<'backend> {
    backend: &'backend mut VulkanRendererBackend,
}

pub(super) trait VulkanSwapchainContext {
    fn swapchain(&mut self) -> VulkanSwapchainOperations<'_>;
}

impl VulkanSwapchainContext for VulkanRendererBackend {
    fn swapchain(&mut self) -> VulkanSwapchainOperations<'_> {
        VulkanSwapchainOperations { backend: self }
    }
}

impl<'backend> VulkanSwapchainOperations<'backend> {
    pub fn create(&mut self, width: u32, height: u32) -> Result<(), String> {
        let mut extents = vk::Extent2D { width, height };
        let device = self.backend.device.as_ref().ok_or("Failed to get Vulkan device")?;
        let surface_loader = self
            .backend
            .surface_loader
            .as_ref()
            .ok_or("Failed to get Vulkan surface loader")?;
        let surface =
            self.backend.surface.as_ref().ok_or("Failed to get Vulkan surface")?.clone();

        // Choose the swapchain surface format
        let formats = &device.swapchain_support.formats;

        let swapchain_format = formats
            .iter()
            .find(|format| {
                format.format == vk::Format::B8G8R8A8_UNORM
                    && format.color_space == vk::ColorSpaceKHR::SRGB_NONLINEAR
            })
            .copied() // Use copied() as vk::SurfaceFormatKHR implements Copy
            .unwrap_or_else(|| formats[0]); // Fallback to the first format if the preferred one isn't found

        // Choose the swapchain present mode
        let present_modes = &device.swapchain_support.present_modes;
        let swapchain_present_mode = present_modes
            .iter()
            .find(|mode| **mode == vk::PresentModeKHR::MAILBOX)
            .copied() // Use copied() as vk::PresentModeKHR implements Copy
            .unwrap_or(vk::PresentModeKHR::FIFO); // Fallback to FIFO if MAILBOX isn't available

        // Requery the swapchain capabilities
        let swapchain_support = match VulkanDevice::query_swapchain_support(
            surface_loader,
            device.physical_device,
            surface,
        ) {
            Ok(swapchain_support) => swapchain_support,
            Err(e) => return Err(format!("Failed to query swapchain support: {}", e)),
        };

        // Swapchain extent
        if swapchain_support.capabilities.current_extent.width != u32::MAX {
            extents = device.swapchain_support.capabilities.current_extent;
        }

        // Clamp the width and height to the swapchain extent
        let min = swapchain_support.capabilities.min_image_extent;
        let max = swapchain_support.capabilities.max_image_extent;
        extents.width = width.clamp(min.width, max.width);
        extents.height = height.clamp(min.height, max.height);

        // Minimum number of images in the swapchain
        let mut image_count = swapchain_support.capabilities.min_image_count + 1;
        if swapchain_support.capabilities.max_image_count > 0
            && image_count > swapchain_support.capabilities.max_image_count
        {
            image_count = swapchain_support.capabilities.max_image_count;
        }

        // Create the swapchain
        let mut swapchain_create_info = vk::SwapchainCreateInfoKHR::default()
            .surface(surface)
            .min_image_count(image_count)
            .image_format(swapchain_format.format)
            .image_color_space(swapchain_format.color_space)
            .image_extent(extents)
            .image_array_layers(1)
            .image_usage(vk::ImageUsageFlags::COLOR_ATTACHMENT);

        // Setup the queue family indices
        let queue_family_indices = [device.graphics_queue_index, device.present_queue_index];
        if device.graphics_queue_index != device.present_queue_index {
            swapchain_create_info = swapchain_create_info
                .image_sharing_mode(vk::SharingMode::CONCURRENT)
                .queue_family_indices(&queue_family_indices);
        } else {
            swapchain_create_info = swapchain_create_info
                .image_sharing_mode(vk::SharingMode::EXCLUSIVE)
                .queue_family_indices(&[]);
        }

        // Set the pre-transform and composite alpha
        swapchain_create_info = swapchain_create_info
            .pre_transform(swapchain_support.capabilities.current_transform)
            .composite_alpha(vk::CompositeAlphaFlagsKHR::OPAQUE)
            .present_mode(swapchain_present_mode)
            .clipped(true);

        let swapchain_loader = self
            .backend
            .swapchain_loader
            .as_ref()
            .ok_or("Failed to get Vulkan swapchain loader")?;

        // Create the swapchain
        let swapchain =
            match unsafe { swapchain_loader.create_swapchain(&swapchain_create_info, None) } {
                Ok(swapchain) => swapchain,
                Err(e) => return Err(format!("Failed to create swapchain: {:?}", e)),
            };

        // Get the swapchain images
        let swapchain_images =
            match unsafe { swapchain_loader.get_swapchain_images(swapchain) } {
                Ok(images) => images,
                Err(e) => return Err(format!("Failed to get swapchain images: {:?}", e)),
            };

        // Create image views for the swapchain images
        let image_views = swapchain_images
            .iter()
            .map(|&image| {
                let view_create_info = vk::ImageViewCreateInfo::default()
                    .image(image)
                    .view_type(vk::ImageViewType::TYPE_2D)
                    .format(swapchain_format.format)
                    .subresource_range(
                        vk::ImageSubresourceRange::default()
                            .aspect_mask(vk::ImageAspectFlags::COLOR)
                            .base_mip_level(0)
                            .level_count(1)
                            .base_array_layer(0)
                            .layer_count(1),
                    );

                match unsafe {
                    device
                        .logical_device
                        .as_ref()
                        .unwrap()
                        .create_image_view(&view_create_info, None)
                } {
                    Ok(view) => Ok(view),
                    Err(e) => Err(format!(
                        "Failed to create image view for swapchain image: {:?}",
                        e
                    )),
                }
            })
            .collect::<Result<Vec<_>, _>>();
        let swapchain_views = match image_views {
            Ok(views) => views,
            Err(e) => return Err(format!("Failed to create image views: {}", e)),
        };

        let instance =
            self.backend.instance.as_ref().ok_or("Failed to get Vulkan instance")?;
        // Detect the depth format
        //TODO: Recover from error ?
        let depth_format = match device.detect_depth_format(instance) {
            Ok(depth_format) => depth_format,
            Err(e) => return Err(format!("Failed to detect depth format: {}", e)),
        };

        // Create the depth image and view
        let depth_attachment = match VulkanImage::new(
            device.logical_device.as_ref().unwrap(),
            device.physical_device,
            instance,
            width,
            height,
            depth_format,
            vk::ImageTiling::OPTIMAL,
            vk::ImageUsageFlags::DEPTH_STENCIL_ATTACHMENT,
            vk::MemoryPropertyFlags::DEVICE_LOCAL,
            true,
            vk::ImageAspectFlags::DEPTH,
        ) {
            Ok(depth_attachment) => depth_attachment,
            Err(e) => return Err(format!("Failed to create depth image: {}", e)),
        };

        let device = self.backend.device.as_mut().ok_or("Failed to get Vulkan device")?;
        self.backend.current_frame = 0;
        device.swapchain_support = swapchain_support;
        device.depth_format = depth_format;

        self.backend.swapchain = Some(VulkanSwapchain {
            max_frames_in_flight: 2,

            surface_format: swapchain_format,
            handle: swapchain,
            images: swapchain_images,
            image_views: swapchain_views,

            depth_attachment,
            framebuffers: vec![],
        });
        Ok(())
    }

    pub fn destroy(&mut self) -> Result<(), String> {
        self.backend.device().wait_for_device_idle()?;

        // Destroy the depth image and view
        let device = device!(self.backend);
        let swapchain = swapchain_mut!(self.backend);
        let swapchain_loader = swapchain_loader!(self.backend);

        swapchain.depth_attachment.destroy(device);

        // Destroy the image views
        for &image_view in swapchain.image_views.iter() {
            unsafe {
                device.destroy_image_view(image_view, None);
            }
        }

        // Destroy the swapchain
        unsafe {
            swapchain_loader.destroy_swapchain(swapchain.handle, None);
        }

        Ok(())
    }

    fn recreate(&mut self, width: u32, height: u32) -> Result<(), String> {
        self.destroy()?;
        self.create(width, height)?;

        Ok(())
    }

    fn acquire_next_image_index(
        &mut self,
        width: u32,
        height: u32,
        timeout_ns: u64,
        image_available_semaphore: &vk::Semaphore,
        fence: Option<vk::Fence>,
    ) -> Result<usize, String> {
        let swapchain =
            self.backend.swapchain.as_ref().ok_or("Failed to get Vulkan swapchain")?;
        let swapchain_loader = self
            .backend
            .swapchain_loader
            .as_ref()
            .ok_or("Failed to get Vulkan swapchain loader")?;

        match unsafe {
            swapchain_loader.acquire_next_image(
                swapchain.handle,
                timeout_ns,
                *image_available_semaphore,
                *fence.as_ref().unwrap_or(&vk::Fence::null()),
            )
        } {
            Ok((index, _)) => Ok(index as usize),
            Err(vk::Result::ERROR_OUT_OF_DATE_KHR) => {
                self.recreate(width, height)?;
                Ok(usize::MAX)
            }
            Err(e) => Err(format!("Failed to acquire next image index: {:?}", e)),
        }
    }

    fn swapchain_present(
        &mut self,
        framebuffer_width: u32,
        framebuffer_height: u32,
        present_queue: vk::Queue,
        render_completed_semaphore: vk::Semaphore,
        image_index: usize,
    ) {
        let swapchain = self.backend.swapchain.as_mut().unwrap();
        let swapchain_loader = self.backend.swapchain_loader.as_ref().unwrap();
        let swapchain_handles = [swapchain.handle];
        let semaphores = [render_completed_semaphore];
        let image_indices = [image_index as u32];

        let present_info = vk::PresentInfoKHR::default()
            .wait_semaphores(&semaphores)
            .swapchains(&swapchain_handles)
            .image_indices(&image_indices);

        match unsafe { swapchain_loader.queue_present(present_queue, &present_info) } {
            Ok(_) => {}
            Err(vk::Result::ERROR_OUT_OF_DATE_KHR) => {
                self.recreate(framebuffer_width, framebuffer_height).unwrap();
            }
            Err(vk::Result::SUBOPTIMAL_KHR) => {
                self.recreate(framebuffer_width, framebuffer_height).unwrap();
            }
            Err(e) => {
                log::error!("Failed to present swapchain: {:?}", e);
            }
        }
    }

    pub fn resize_swapchain_if_needed(&mut self) -> Result<bool, String> {
        // Check if recreating the swapchain is in progress and skip the frame is so.
        if self.backend.recreating_swapchain {
            // Wait for the device to be idle before resizing.
            let device = device!(self.backend);
            match unsafe { device.device_wait_idle() } {
                Ok(_) => {}
                Err(err) => return Err(format!("Failed to wait for device idle: {:?}", err)),
            }

            log::debug!("Recreating swapchain, skipping frame");
            return Ok(false); // Skip the frame.
        }

        // Check if the framebuffer size has changed.
        if self.backend.framebuffer_size_generation
            != self.backend.framebuffer_size_last_generation
        {
            let width = self.backend.framebuffer_width;
            let height = self.backend.framebuffer_height;
            log::debug!("Framebuffer size changed: {}x{}", width, height);

            // Wait for the device to be idle before resizing.
            let device = device!(self.backend);
            match unsafe { device.device_wait_idle() } {
                Ok(_) => {}
                Err(err) => return Err(format!("Failed to wait for device idle: {:?}", err)),
            }

            // Recreate the swapchain.
            match self.recreate(width, height) {
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
        let current_frame = self.backend.current_frame;
        match self.backend.fence().wait_for_image_fence(current_frame) {
            Ok(_) => {}
            Err(err) => return Err(format!("Failed to wait for image fence: {:?}", err)),
        }

        let width = self.backend.framebuffer_width;
        let height = self.backend.framebuffer_height;
        let semaphore = self.backend.image_available_semaphores[current_frame];
        match self.acquire_next_image_index(width, height, u64::MAX, &semaphore, None) {
            Ok(index) => {
                self.backend.image_index = index;
            }
            Err(e) => {
                log::warn!("Failed to acquire image index: {:?}", e);
                return Ok(false); // Skip the frame.
            }
        }

        Ok(true) // Continue with the frame.
    }

    pub fn present(&mut self) -> Result<(), String> {
        let width = self.backend.framebuffer_width;
        let height = self.backend.framebuffer_height;
        let present_queue = vulkan_device!(self.backend).present_queue;
        let render_completed_semaphore =
            self.backend.queue_complete_semaphores[self.backend.current_frame];
        let image_index = self.backend.image_index;

        self.swapchain_present(
            width,
            height,
            present_queue,
            render_completed_semaphore,
            image_index,
        );

        self.backend.current_frame =
            (self.backend.current_frame + 1) % swapchain!(self.backend).max_frames_in_flight;
        Ok(())
    }

    pub fn recreate_swapchain(&mut self, width: u32, height: u32) -> Result<(), String> {
        // If already recreating, skip
        if self.backend.recreating_swapchain {
            return Ok(());
        }

        // Detect if the window is too small to create a swapchain
        if self.backend.framebuffer_width == 0 || self.backend.framebuffer_height == 0 {
            return Ok(());
        }

        // Set the recreating flag to true
        self.backend.recreating_swapchain = true;

        // Wait for the device to be idle before resizing.
        self.backend.device().wait_for_device_idle()?;

        // Clear images just in case
        for image in self.backend.images_in_flight.iter_mut() {
            *image = None;
        }

        // Requery swapchain support details
        {
            let phys_device = vulkan_device!(self.backend).physical_device;
            let surface = surface!(self.backend);
            let surface_loader = surface_loader!(self.backend);

            vulkan_device_mut!(self.backend).swapchain_support =
                VulkanDevice::query_swapchain_support(surface_loader, phys_device, *surface)?;

            vulkan_device!(self.backend).detect_depth_format(instance!(self.backend))?;

            self.recreate(width, height)?;
        }

        {
            let renderpass = main_renderpass_mut!(self.backend);
            self.backend.framebuffer_width = width;
            self.backend.framebuffer_height = height;
            renderpass.w = width as f32;
            renderpass.h = height as f32;
            renderpass.x = 0.0;
            renderpass.y = 0.0;

            self.backend.framebuffer_size_last_generation =
                self.backend.framebuffer_size_generation;
        }

        // Cleanup
        {
            for buf in self.backend.graphics_cmds_buffers.iter_mut() {
                buf.destroy(
                    device!(self.backend),
                    vulkan_device!(self.backend).graphics_command_pool,
                );
            }
        }

        {
            for buf in swapchain_mut!(self.backend).framebuffers.iter_mut() {
                buf.destroy(device!(self.backend));
            }

            self.backend.framebuffer().recreate_framebuffers()?;
        }

        self.backend.commands().create_commands_buffers()?;

        // Reset the recreating flag
        self.backend.recreating_swapchain = false;

        Ok(())
    }
}
