use ash::vk;

use super::{VulkanContext, device::VulkanDevice, image::VulkanImage};

pub(super) struct VulkanSwapchain {
    pub surface_format: vk::SurfaceFormatKHR,
    max_frames_in_flight: u8,
    handle: vk::SwapchainKHR,
    pub images: Vec<vk::Image>,
    image_views: Vec<vk::ImageView>,
    depth_attachment: VulkanImage,
}

impl VulkanSwapchain {
    pub(super) fn new(
        context: &mut VulkanContext,
        width: u32,
        height: u32,
    ) -> Result<Self, String> {
        Self::create_swapchain(context, width, height)
    }

    pub(super) fn recreate(
        &mut self,
        context: &mut VulkanContext,
        width: u32,
        height: u32,
    ) -> Result<(), String> {
        Self::destroy_swapchain(context, self);
        *self = match Self::create_swapchain(context, width, height) {
            Ok(swapchain) => swapchain,
            Err(e) => return Err(format!("Failed to recreate swapchain: {}", e)),
        };
        Ok(())
    }

    pub(super) fn destroy(&mut self, context: &VulkanContext) {
        Self::destroy_swapchain(context, self);
    }

    pub(super) fn acquire_next_image_index(
        &mut self,
        context: &mut VulkanContext,
        timeout_ns: u64,
        image_available_semaphore: vk::Semaphore,
        fence: vk::Fence,
    ) -> Result<u32, String> {
        match unsafe {
            context.swapchain_loader.as_ref().unwrap().acquire_next_image(
                self.handle,
                timeout_ns,
                image_available_semaphore,
                fence,
            )
        } {
            Ok((index, _)) => Ok(index),
            Err(vk::Result::ERROR_OUT_OF_DATE_KHR) => {
                self.recreate(
                    context,
                    context.framebuffer_width,
                    context.framebuffer_height,
                )?;
                Ok(u32::MAX)
            }
            Err(e) => Err(format!("Failed to acquire next image index: {:?}", e)),
        }
    }

    pub(super) fn present(
        &mut self,
        context: &mut VulkanContext,
        _graphics_queue: vk::Queue,
        present_queue: vk::Queue,
        render_completed_semaphore: vk::Semaphore,
        image_index: u32,
    ) {
        let semaphores = [render_completed_semaphore];
        let swapchains = [self.handle];
        let image_indices = [image_index];
        let present_info = vk::PresentInfoKHR::default()
            .wait_semaphores(&semaphores)
            .swapchains(&swapchains)
            .image_indices(&image_indices);
        match unsafe {
            context
                .swapchain_loader
                .as_ref()
                .unwrap()
                .queue_present(present_queue, &present_info)
        } {
            Ok(_) => {}
            Err(vk::Result::ERROR_OUT_OF_DATE_KHR) => match self.recreate(
                context,
                context.framebuffer_width,
                context.framebuffer_height,
            ) {
                Ok(_) => {}
                Err(e) => log::error!("Failed to recreate swapchain: {}", e),
            },
            Err(vk::Result::SUBOPTIMAL_KHR) => {
                match self.recreate(
                    context,
                    context.framebuffer_width,
                    context.framebuffer_height,
                ) {
                    Ok(_) => {}
                    Err(e) => log::error!("Failed to recreate swapchain: {}", e),
                }
            }
            Err(e) => {
                log::error!("Failed to present swapchain: {:?}", e);
            }
        }
    }

    fn create_swapchain(
        context: &mut VulkanContext,
        width: u32,
        height: u32,
    ) -> Result<Self, String> {
        let mut extents = vk::Extent2D { width, height };

        // Choose the swapchain surface format
        let device = context.device.as_ref().unwrap();
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
            context,
            device.physical_device,
            context.surface.clone().unwrap(),
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
            .surface(context.surface.clone().unwrap())
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

        // Create the swapchain
        let swapchain = match unsafe {
            context
                .swapchain_loader
                .as_ref()
                .unwrap()
                .create_swapchain(&swapchain_create_info, None)
        } {
            Ok(swapchain) => swapchain,
            Err(e) => return Err(format!("Failed to create swapchain: {:?}", e)),
        };

        // Get the swapchain images
        let swapchain_images = match unsafe {
            context.swapchain_loader.as_ref().unwrap().get_swapchain_images(swapchain)
        } {
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
                    context
                        .device
                        .as_ref()
                        .unwrap()
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

        // Detect the depth format
        //TODO: Recover from error ?
        let depth_format = match device.detect_depth_format(context) {
            Ok(depth_format) => depth_format,
            Err(e) => return Err(format!("Failed to detect depth format: {}", e)),
        };

        // Create the depth image and view
        let depth_attachment = match VulkanImage::new(
            context,
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

        let device = context.device.as_mut().unwrap();
        context.current_frame = 0;
        device.swapchain_support = swapchain_support;
        device.depth_format = depth_format;

        Ok(Self {
            max_frames_in_flight: 2,

            surface_format: swapchain_format,
            handle: swapchain,
            images: swapchain_images,
            image_views: swapchain_views,

            depth_attachment,
        })
    }

    fn destroy_swapchain(context: &VulkanContext, swapchain: &mut Self) {
        let device = context.device.as_ref().unwrap().logical_device.as_ref().unwrap();
        // Destroy the depth image and view
        swapchain.depth_attachment.destroy(context);

        // Destroy the image views
        for &image_view in swapchain.image_views.iter() {
            unsafe {
                device.destroy_image_view(image_view, None);
            }
        }

        // Destroy the swapchain
        unsafe {
            context
                .swapchain_loader
                .as_ref()
                .unwrap()
                .destroy_swapchain(swapchain.handle, None);
        }
    }
}
