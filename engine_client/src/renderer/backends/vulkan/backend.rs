//! Vulkan backend implementation - backend.rs

use super::context::VulkanContext;
use super::device::VulkanDevice;
use super::fence::VulkanFence;
use super::framebuffer::VulkanFramebuffer;
use super::renderpass::VulkanRenderPass;
use super::swapchain::VulkanSwapchain;
use crate::platform::WindowHandle;
use crate::renderer::RendererBackend;
use crate::renderer::backends::vulkan::swapchain::VulkanSwapchainContext;
use ash::Entry;
use ash::vk;
use raw_window_handle::{HasDisplayHandle, HasWindowHandle};
use std::borrow::Cow;
// RendererBackendVulkan definition and implementation moved from mod.rs

pub struct RendererBackendVulkan {
    // Vulkan context
    // Holds the Vulkan context state for this backend.
    context: Option<VulkanContext>,

    cached_framebuffer_width: u32,
    cached_framebuffer_height: u32,
}

impl RendererBackendVulkan {
    /// Creates a new Vulkan renderer backend instance.
    ///
    /// # Returns
    /// * `RendererBackendVulkan` - A new Vulkan backend instance.
    pub fn new() -> Self {
        Self {
            context: None,
            cached_framebuffer_width: 0,
            cached_framebuffer_height: 0,
        }
    }
}

impl RendererBackend for RendererBackendVulkan {
    fn initialize(&mut self, window_hndl: WindowHandle) -> Result<(), String> {
        // Initialize Vulkan resources here
        // if window_hndl.window.display_handle().is_err() {
        //     return Err("Window handle is invalid".to_string());
        // }
        let window = match window_hndl.window.upgrade() {
            Some(window) => match window.read() {
                Ok(window) => window.clone(),
                Err(_) => return Err("Failed to lock window".to_string()),
            },
            None => return Err("Window handle is invalid".to_string()),
        };
        let (width, height) = window.size();
        self.cached_framebuffer_width = width;
        self.cached_framebuffer_height = height;

        unsafe {
            log::debug!("Initializing Vulkan renderer");

            let entry = Entry::linked();

            // Create Vulkan Context
            let mut context = VulkanContext::new();

            context.framebuffer_width = width;
            context.framebuffer_height = height;

            // Create Vulkan Application Info
            let app_infos = vk::ApplicationInfo::default()
                .application_name(c"Void Architect")
                .engine_name(c"Void Architect Engine")
                .application_version(0)
                .engine_version(0)
                .api_version(vk::make_api_version(0, 1, 2, 0));

            let layer_names = vec![c"VK_LAYER_KHRONOS_validation".as_ptr()];

            let mut extension_names = ash_window::enumerate_required_extensions(
                window.display_handle().unwrap().as_raw(),
            )
            .unwrap()
            .to_vec();
            // let mut extension_names = match window.vulkan_instance_extensions() {
            //     Ok(extensions) => {
            //         extensions.iter().map(|s| s.as_ptr() as *const i8).collect::<Vec<_>>()
            // `   }
            //     Err(e) => {
            //         log::error!("Failed to get Vulkan instance extensions: {:?}", e);
            //         return Err(format!("Failed to get Vulkan instance extensions: {:?}", e));
            //     }
            // };

            extension_names.push(ash::ext::debug_utils::NAME.as_ptr());
            extension_names.push(ash::ext::debug_report::NAME.as_ptr());

            #[cfg(any(target_os = "macos", target_os = "ios"))]
            {
                extension_names.push(ash::khr::portability_enumeration::NAME.as_ptr());
                extension_names.push(ash::khr::get_physical_device_properties2::NAME.as_ptr());
            }

            let create_flags = if cfg!(any(target_os = "macos", target_os = "ios")) {
                vk::InstanceCreateFlags::ENUMERATE_PORTABILITY_KHR
            } else {
                vk::InstanceCreateFlags::empty()
            };

            // Initialize Vulkan instance, device, etc.
            let instance_create_info = vk::InstanceCreateInfo::default()
                .application_info(&app_infos)
                .enabled_layer_names(&layer_names)
                .enabled_extension_names(&extension_names)
                .flags(create_flags);

            context.instance = match entry.create_instance(&instance_create_info, None) {
                Ok(instance) => Some(instance),
                Err(e) => {
                    log::error!("Failed to create Vulkan instance: {:?}", e);
                    return Err(format!("Failed to create Vulkan instance: {:?}", e));
                }
            };
            log::debug!("Vulkan instance created successfully");

            // If this is a debug build, create debug messenger
            #[cfg(debug_assertions)]
            {
                let debug_info = vk::DebugUtilsMessengerCreateInfoEXT::default()
                    .message_severity(
                        vk::DebugUtilsMessageSeverityFlagsEXT::ERROR
                            | vk::DebugUtilsMessageSeverityFlagsEXT::WARNING
                            | vk::DebugUtilsMessageSeverityFlagsEXT::INFO,
                    )
                    .message_type(
                        vk::DebugUtilsMessageTypeFlagsEXT::GENERAL
                            | vk::DebugUtilsMessageTypeFlagsEXT::VALIDATION
                            | vk::DebugUtilsMessageTypeFlagsEXT::PERFORMANCE,
                    )
                    .pfn_user_callback(Some(vulkan_debug_callback));

                context.debug_utils_loader = Some(ash::ext::debug_utils::Instance::new(
                    &entry,
                    &context.instance.as_ref().unwrap(),
                ));
                context.debug_callback = Some(
                    context
                        .debug_utils_loader
                        .as_ref()
                        .expect("Vulkan debugger loader not initialized")
                        .create_debug_utils_messenger(&debug_info, None)
                        .expect("Debug callback creation failed"),
                );
                log::debug!("Vulkan debug callback created successfully");
            }

            // Create Vulkan surface
            let surface = match ash_window::create_surface(
                &entry,
                &context.instance.as_ref().unwrap(),
                window.display_handle().unwrap().as_raw(),
                window.window_handle().unwrap().as_raw(),
                None,
            ) {
                Ok(surface) => surface,
                Err(e) => {
                    log::error!("Failed to create Vulkan surface: {:?}", e);
                    return Err(format!("Failed to create Vulkan surface: {:?}", e));
                }
            };
            // let surface = match window.vulkan_create_surface(
            //     context.instance.as_mut().unwrap().handle().as_raw() as *mut __VkInstance,
            // ) {
            //     Ok(surface) => vk::SurfaceKHR::from_raw(surface as u64),
            //     Err(e) => {
            //         log::error!("Failed to create Vulkan surface: {:?}", e);
            //         return Err(format!("Failed to create Vulkan surface: {:?}", e));
            //     }
            // };
            context.surface = Some(surface);
            log::debug!("Vulkan surface created successfully");

            // Create Vulkan surface loader
            context.surface_loader = Some(ash::khr::surface::Instance::new(
                &entry,
                &context.instance.as_ref().unwrap(),
            ));
            log::debug!("Vulkan surface loader created successfully");

            // Create Vulkan device
            context.device = match VulkanDevice::new(
                context.instance.as_ref().ok_or("Vulkan instance not initialized")?,
                context
                    .surface_loader
                    .as_ref()
                    .ok_or("Vulkan surface loader not initialized")?,
                surface,
            ) {
                Ok(device) => Some(device),
                Err(e) => {
                    log::error!("Failed to create Vulkan device: {:?}", e);
                    return Err(format!("Failed to create Vulkan device: {:?}", e));
                }
            };
            log::debug!("Vulkan device created successfully");

            // Create Vulkan swapchain loader
            context.swapchain_loader = Some(ash::khr::swapchain::Device::new(
                context.instance.as_ref().unwrap(),
                context.device.as_ref().unwrap().logical_device.as_ref().unwrap(),
            ));
            log::debug!("Vulkan swapchain loader created successfully");

            // Create Vulkan swapchain
            //let width = context.framebuffer_width;
            //let height = context.framebuffer_height;
            match context.swapchain().create(width, height) {
                Ok(_) => {}
                Err(e) => {
                    log::error!("Failed to create Vulkan swapchain: {:?}", e);
                    return Err(format!("Failed to create Vulkan swapchain: {:?}", e));
                }
            };
            log::debug!("Vulkan swapchain created successfully");

            // Create the main render pass
            context.main_renderpass = match VulkanRenderPass::new(
                &context,
                0.0,
                0.0,
                width as f32,
                height as f32,
                0.0,
                0.0,
                0.2,
                1.0,
                1.0,
                0,
            ) {
                Ok(render_pass) => Some(render_pass),
                Err(e) => {
                    log::error!("Failed to create Vulkan render pass: {:?}", e);
                    return Err(format!("Failed to create Vulkan render pass: {:?}", e));
                }
            };
            log::debug!("Vulkan render pass created successfully");

            // Create Vulkan framebuffers

            match context.recreate_framebuffers() {
                Ok(_) => {}
                Err(e) => {
                    log::error!("Failed to create Vulkan framebuffers: {:?}", e);
                    return Err(format!("Failed to create Vulkan framebuffers: {:?}", e));
                }
            };

            context.framebuffer_width = width;
            context.framebuffer_height = height;

            // Create command buffers
            match context.create_commands_buffers() {
                Ok(_) => {}
                Err(e) => {
                    log::error!("Failed to create Vulkan command buffers: {:?}", e);
                    return Err(format!("Failed to create Vulkan command buffers: {:?}", e));
                }
            };
            log::debug!("Vulkan command buffers created successfully");

            let device = context.device.as_ref().unwrap().logical_device.as_ref().unwrap();
            let swapchain = context.swapchain.as_mut().unwrap();

            // Create semaphores and fences
            for _ in 0..swapchain.max_frames_in_flight {
                let image_available_semaphore =
                    match device.create_semaphore(&vk::SemaphoreCreateInfo::default(), None) {
                        Ok(semaphore) => semaphore,
                        Err(e) => {
                            log::error!("Failed to create image available semaphore: {:?}", e);
                            return Err(format!(
                                "Failed to create image available semaphore: {:?}",
                                e
                            ));
                        }
                    };
                context.image_available_semaphores.push(image_available_semaphore);

                let queue_complete_semaphore =
                    match device.create_semaphore(&vk::SemaphoreCreateInfo::default(), None) {
                        Ok(semaphore) => semaphore,
                        Err(e) => {
                            log::error!("Failed to create queue complete semaphore: {:?}", e);
                            return Err(format!(
                                "Failed to create queue complete semaphore: {:?}",
                                e
                            ));
                        }
                    };
                context.queue_complete_semaphores.push(queue_complete_semaphore);

                let fence = match VulkanFence::new(device, true) {
                    Ok(fence) => fence,
                    Err(e) => {
                        log::error!("Failed to create fence: {:?}", e);
                        return Err(format!("Failed to create fence: {:?}", e));
                    }
                };
                context.in_flight_fences.push(fence);
            }

            for _ in 0..swapchain.images.len() {
                context.images_in_flight.push(None);
            }
            log::debug!("Vulkan semaphores and fences created successfully");

            self.context = Some(context);
            log::debug!("Vulkan renderer initialized successfully");
            Ok(())
        }
    }

    fn shutdown(&mut self) -> Result<(), String> {
        // Cleanup Vulkan resources here
        if let Some(mut context) = self.context.take() {
            log::debug!("Shutting down Vulkan renderer");
            context.wait_for_device_idle()?;

            // Destroy Vulkan fences
            let device = context.device.as_ref().unwrap().logical_device.as_ref().unwrap();
            for fence in context.in_flight_fences.iter() {
                fence.destroy(device);
            }
            context.in_flight_fences.clear();
            log::debug!("Vulkan fences destroyed");

            // Destroy Vulkan semaphores
            for semaphore in context.image_available_semaphores.iter() {
                unsafe { device.destroy_semaphore(*semaphore, None) };
            }
            context.image_available_semaphores.clear();
            for semaphore in context.queue_complete_semaphores.iter() {
                unsafe { device.destroy_semaphore(*semaphore, None) };
            }
            context.queue_complete_semaphores.clear();
            log::debug!("Vulkan semaphores destroyed");

            // Destroy Vulkan command buffers
            match context.destroy_commands_buffers() {
                Ok(_) => {}
                Err(e) => {
                    log::error!("Failed to destroy Vulkan command buffers: {:?}", e);
                    return Err(format!("Failed to destroy Vulkan command buffers: {:?}", e));
                }
            }
            log::debug!("Vulkan command buffers destroyed");

            // Destroy Vulkan framebuffers
            if let Some(swapchain) = context.swapchain.as_ref() {
                for framebuffer in swapchain.framebuffers.iter() {
                    framebuffer.destroy(
                        context.device.as_ref().unwrap().logical_device.as_ref().unwrap(),
                    );
                }
                log::debug!("Vulkan framebuffers destroyed");
            }

            // Destroy Vulkan render pass
            if let Some(render_pass) = context.main_renderpass.take() {
                render_pass.destroy(&context);
                log::debug!("Vulkan render pass destroyed");
            }

            // Destroy Vulkan swapchain
            if context.swapchain.is_some() {
                context.swapchain().destroy()?;
                log::debug!("Vulkan swapchain destroyed");
            }

            // Destroy Vulkan device
            if let Some(mut device) = context.device.take() {
                device.destroy();
                log::debug!("Vulkan device destroyed");
            }

            // Destroy Vulkan surface
            if let Some(surface) = context.surface {
                unsafe {
                    context.surface_loader.as_ref().unwrap().destroy_surface(surface, None);
                    log::debug!("Vulkan surface destroyed");
                }
            }

            // If this is a debug build, destroy debug messenger
            #[cfg(debug_assertions)]
            {
                if let Some(debug_callback) = context.debug_callback {
                    unsafe {
                        context
                            .debug_utils_loader
                            .as_ref()
                            .unwrap()
                            .destroy_debug_utils_messenger(debug_callback, None);
                        log::debug!("Vulkan debug callback destroyed");
                    }
                }
            }

            if let Some(instance) = context.instance {
                unsafe {
                    instance.destroy_instance(None);
                    log::debug!("Vulkan instance destroyed");
                }
            }
        }
        log::debug!("Vulkan renderer shut down successfully");
        Ok(())
    }

    fn begin_frame(&mut self) -> Result<bool, String> {
        // Begin Vulkan frame rendering here
        if self.context.is_none() {
            return Err("Vulkan context is not initialized".to_string());
        }

        let ctx = self.context.as_mut().ok_or("Context not initialized")?;

        // Check if we need to resize the swapchain, if yes, resize it and skip the frame.
        if !ctx.resize_swapchain_if_needed()? {
            return Ok(false); // Skip this frame.
        }

        // Acquire the next image from the swapchain
        ctx.acquire_next_image()?;

        // Begin command buffer recording
        ctx.begin_gfx_commands_buffer()?;

        // Dynamic state
        ctx.begin_main_render_pass()?;

        Ok(true)
    }

    fn end_frame(&mut self) -> Result<(), String> {
        // End Vulkan frame rendering here
        if self.context.is_none() {
            return Err("Vulkan context is not initialized".to_string());
        }

        // End the renderpass
        let ctx = self.context.as_mut().ok_or("Context not initialized")?;
        ctx.end_main_render_pass()?;
        ctx.end_gfx_commands_buffer()?;

        ctx.wait_for_previous_frame_to_complete()?;

        ctx.update_image_sync()?;

        ctx.submit_gfx_command_buf()?;
        ctx.present()?;

        Ok(())
    }

    fn resize(&mut self, width: u32, height: u32) -> Result<(), String> {
        // Handle Vulkan swapchain resizing here
        if width == 0 || height == 0 {
            return Err("Invalid width or height".to_string());
        }

        self.cached_framebuffer_width = width;
        self.cached_framebuffer_height = height;

        let mut current_gen = 0;
        if let Some(context) = &mut self.context {
            context.framebuffer_size_generation += 1;
            current_gen = context.framebuffer_size_generation;
        }

        log::debug!(
            "Vulkan swapchain resized to {}x{}, gen {}",
            width,
            height,
            current_gen
        );

        Ok(())
    }
}

#[cfg(debug_assertions)]
unsafe extern "system" fn vulkan_debug_callback(
    message_severity: vk::DebugUtilsMessageSeverityFlagsEXT,
    message_type: vk::DebugUtilsMessageTypeFlagsEXT,
    p_callback_data: *const vk::DebugUtilsMessengerCallbackDataEXT<'_>,
    _p_user_data: *mut std::ffi::c_void,
) -> vk::Bool32 {
    unsafe {
        let callback_data = *p_callback_data;
        let message_id_number = callback_data.message_id_number;

        let message_id_name = if callback_data.p_message_id_name.is_null() {
            Cow::from("")
        } else {
            std::ffi::CStr::from_ptr(callback_data.p_message_id_name).to_string_lossy()
        };

        let message = if callback_data.p_message.is_null() {
            Cow::from("")
        } else {
            std::ffi::CStr::from_ptr(callback_data.p_message).to_string_lossy()
        };

        match message_severity {
            vk::DebugUtilsMessageSeverityFlagsEXT::ERROR => {
                log::error!(
                    "Vulkan:{:?} [{}: {}] = {}",
                    message_type,
                    message_id_name,
                    message_id_number,
                    message
                );
            }
            vk::DebugUtilsMessageSeverityFlagsEXT::WARNING => {
                log::warn!(
                    "Vulkan:{:?} [{}: {}] = {}",
                    message_type,
                    message_id_name,
                    message_id_number,
                    message
                );
            }
            vk::DebugUtilsMessageSeverityFlagsEXT::INFO => {
                log::info!(
                    "Vulkan:{:?} [{}: {}] = {}",
                    message_type,
                    message_id_name,
                    message_id_number,
                    message
                );
            }
            _ => {}
        }

        vk::FALSE
    }
}
