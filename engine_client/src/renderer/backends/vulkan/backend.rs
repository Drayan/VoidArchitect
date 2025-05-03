use ash::{Entry, vk};
use raw_window_handle::{HasDisplayHandle, HasWindowHandle};

use crate::renderer::backends::vulkan::buffers::VulkanBufferContext;
use crate::renderer::backends::vulkan::shaders::VulkanShaderModuleContext;
use crate::{
    command_buffer, command_buffer_mut, device, graphics_pipeline, instance, surface_loader,
    swapchain, swapchain_mut, vulkan_device,
};
use crate::{platform::WindowHandle, renderer::RendererBackend};

use super::commands::VulkanCommandBufferContext;
use super::device::VulkanDevice;
use super::device::VulkanDeviceContext;
use super::fence::{VulkanFence, VulkanFenceContext};
use super::framebuffer::VulkanFramebufferContext;
use super::pipeline::VulkanPipelineContext;
use super::renderpass::{VulkanRenderPass, VulkanRenderPassContext};
use super::swapchain::VulkanSwapchainContext;
use super::{VulkanRendererBackend, device};

impl RendererBackend for VulkanRendererBackend {
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
            self.framebuffer_width = width;
            self.framebuffer_height = height;

            // Create Vulkan Application Info
            let app_infos = vk::ApplicationInfo::default()
                .application_name(c"Void Architect")
                .engine_name(c"Void Architect Engine")
                .application_version(0)
                .engine_version(0)
                .api_version(vk::make_api_version(0, 1, 2, 0));

            let layer_names = vec![c"VK_LAYER_KHRONOS_validation".as_ptr()];

            let display_handle = match window.display_handle() {
                Ok(display_hndl) => display_hndl.as_raw(),
                Err(e) => {
                    log::error!("Failed to get display handle: {:?}", e);
                    return Err(format!("Failed to get display handle: {:?}", e));
                }
            };
            let mut extension_names =
                match ash_window::enumerate_required_extensions(display_handle) {
                    Ok(extension_names) => extension_names.to_vec(),
                    Err(e) => {
                        log::error!("Failed to enumerate required extensions: {:?}", e);
                        return Err(format!(
                            "Failed to enumerate required extensions: {:?}",
                            e
                        ));
                    }
                };

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

            self.instance = match entry.create_instance(&instance_create_info, None) {
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

                self.debug_utils_loader = Some(ash::ext::debug_utils::Instance::new(
                    &entry,
                    &self.instance.as_ref().unwrap(),
                ));
                self.debug_callback = match self
                    .debug_utils_loader
                    .as_ref()
                    .ok_or("Debug utils loader not initialized")?
                    .create_debug_utils_messenger(&debug_info, None)
                {
                    Ok(debug_callback) => Some(debug_callback),
                    Err(e) => {
                        log::error!("Failed to create Vulkan debug messenger: {:?}", e);
                        return Err(format!(
                            "Failed to create Vulkan debug messenger: {:?}",
                            e
                        ));
                    }
                };
                log::debug!("Vulkan debug callback created successfully");
            }

            // Create Vulkan surface

            let window_handle = match window.window_handle() {
                Ok(window_hndl) => window_hndl.as_raw(),
                Err(e) => {
                    log::error!("Failed to get window handle: {:?}", e);
                    return Err(format!("Failed to get window handle: {:?}", e));
                }
            };
            let surface = match ash_window::create_surface(
                &entry,
                instance!(self),
                display_handle,
                window_handle,
                None,
            ) {
                Ok(surface) => surface,
                Err(e) => {
                    log::error!("Failed to create Vulkan surface: {:?}", e);
                    return Err(format!("Failed to create Vulkan surface: {:?}", e));
                }
            };
            self.surface = Some(surface);
            log::debug!("Vulkan surface created successfully");

            // Create Vulkan surface loader
            self.surface_loader =
                Some(ash::khr::surface::Instance::new(&entry, instance!(self)));
            log::debug!("Vulkan surface loader created successfully");

            // Create Vulkan device
            self.device =
                match VulkanDevice::new(instance!(self), surface_loader!(self), surface) {
                    Ok(device) => Some(device),
                    Err(e) => {
                        log::error!("Failed to create Vulkan device: {:?}", e);
                        return Err(format!("Failed to create Vulkan device: {:?}", e));
                    }
                };
            log::debug!("Vulkan device created successfully");

            // Create Vulkan swapchain loader
            self.swapchain_loader = Some(ash::khr::swapchain::Device::new(
                instance!(self),
                device!(self),
            ));
            log::debug!("Vulkan swapchain loader created successfully");

            // Create Vulkan swapchain
            //let width = self.framebuffer_width;
            //let height = self.framebuffer_height;
            match self.swapchain().create(width, height) {
                Ok(_) => {}
                Err(e) => {
                    log::error!("Failed to create Vulkan swapchain: {:?}", e);
                    return Err(format!("Failed to create Vulkan swapchain: {:?}", e));
                }
            };
            log::debug!("Vulkan swapchain created successfully");

            // Create the main render pass
            self.main_renderpass = match VulkanRenderPass::new(
                device!(self),
                swapchain!(self).surface_format.format,
                vulkan_device!(self).depth_format,
                0.0,
                0.0,
                width as f32,
                height as f32,
                0.694,
                0.760,
                0.113,
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

            match self.framebuffer().recreate_framebuffers() {
                Ok(_) => {}
                Err(e) => {
                    log::error!("Failed to create Vulkan framebuffers: {:?}", e);
                    return Err(format!("Failed to create Vulkan framebuffers: {:?}", e));
                }
            };

            self.framebuffer_width = width;
            self.framebuffer_height = height;

            // Create command buffers
            match self.commands().create_commands_buffers() {
                Ok(_) => {}
                Err(e) => {
                    log::error!("Failed to create Vulkan command buffers: {:?}", e);
                    return Err(format!("Failed to create Vulkan command buffers: {:?}", e));
                }
            };
            log::debug!("Vulkan command buffers created successfully");

            let device = device!(self);
            let swapchain = swapchain_mut!(self);

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
                self.image_available_semaphores.push(image_available_semaphore);

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
                self.queue_complete_semaphores.push(queue_complete_semaphore);

                let fence = match VulkanFence::new(device, true) {
                    Ok(fence) => fence,
                    Err(e) => {
                        log::error!("Failed to create fence: {:?}", e);
                        return Err(format!("Failed to create fence: {:?}", e));
                    }
                };
                self.in_flight_fences.push(fence);
            }

            for _ in 0..swapchain.images.len() {
                self.images_in_flight.push(None);
            }
            log::debug!("Vulkan semaphores and fences created successfully");

            // Load built-in shaders
            match self.shader_module().load_builtin_shaders() {
                Ok(_) => {}
                Err(e) => {
                    log::error!("Failed to load built-in shaders: {:?}", e);
                    return Err(format!("Failed to load built-in shaders: {:?}", e));
                }
            };
            log::debug!("Vulkan built-in shaders loaded successfully");

            // Create the graphics pipeline
            match self.pipeline().create_graphics_pipeline() {
                Ok(_) => {}
                Err(e) => {
                    log::error!("Failed to create Vulkan graphics pipeline: {:?}", e);
                    return Err(format!(
                        "Failed to create Vulkan graphics pipeline: {:?}",
                        e
                    ));
                }
            };
            log::debug!("Vulkan graphics pipeline created successfully");

            //TODO: --- TEMP CODE - REMOVE THIS LATER ---
            let vertices = vec![
                glam::Vec3::new(-0.5, -0.5, 0.0),
                glam::Vec3::new(0.5, -0.5, 0.0),
                glam::Vec3::new(0.0, 0.5, 0.0),
            ];
            let indices = vec![0, 1, 2];
            match self.buffer().create_vertices_buffer(vertices) {
                Ok(_) => {}
                Err(e) => {
                    log::error!("Failed to create vertices buffer: {:?}", e);
                    return Err(format!("Failed to create vertices buffer: {:?}", e));
                }
            }
            log::debug!("Vulkan vertices buffer created successfully");

            match self.buffer().create_indices_buffer(indices) {
                Ok(_) => {}
                Err(e) => {
                    log::error!("Failed to create indices buffer: {:?}", e);
                    return Err(format!("Failed to create indices buffer: {:?}", e));
                }
            }
            log::debug!("Vulkan indices buffer created successfully");
            //TODO: --- END TEMP CODE ---

            log::debug!("Vulkan renderer initialized successfully");
            Ok(())
        }
    }

    fn shutdown(&mut self) -> Result<(), String> {
        // Cleanup Vulkan resources here
        log::debug!("Shutting down Vulkan renderer");
        self.device().wait_for_device_idle()?;

        // Destroy Vulkan buffers
        match self.buffer().destroy_buffers() {
            Ok(_) => {}
            Err(e) => {
                log::error!("Failed to destroy Vulkan buffers: {:?}", e);
                return Err(format!("Failed to destroy Vulkan buffers: {:?}", e));
            }
        }
        log::debug!("Vulkan buffers destroyed");

        // Destroy Vulkan pipeline
        match self.pipeline().destroy_graphics_pipeline() {
            Ok(_) => {}
            Err(e) => {
                log::error!("Failed to destroy Vulkan pipeline: {:?}", e);
                return Err(format!("Failed to destroy Vulkan pipeline: {:?}", e));
            }
        }
        log::debug!("Vulkan graphics pipeline destroyed");

        // Destroy built-in shaders
        match self.shader_module().destroy_builtin_shaders() {
            Ok(_) => {}
            Err(e) => {
                log::error!("Failed to destroy built-in shaders: {:?}", e);
                return Err(format!("Failed to destroy built-in shaders: {:?}", e));
            }
        }
        log::debug!("Vulkan built-in shaders destroyed");

        // Destroy Vulkan fences
        for fence in self.in_flight_fences.iter() {
            fence.destroy(device!(self));
        }
        self.in_flight_fences.clear();
        log::debug!("Vulkan fences destroyed");

        // Destroy Vulkan semaphores
        for semaphore in self.image_available_semaphores.iter() {
            unsafe { device!(self).destroy_semaphore(*semaphore, None) };
        }
        self.image_available_semaphores.clear();
        for semaphore in self.queue_complete_semaphores.iter() {
            unsafe { device!(self).destroy_semaphore(*semaphore, None) };
        }
        self.queue_complete_semaphores.clear();
        log::debug!("Vulkan semaphores destroyed");

        // Destroy Vulkan command buffers
        match self.commands().destroy_commands_buffers() {
            Ok(_) => {}
            Err(e) => {
                log::error!("Failed to destroy Vulkan command buffers: {:?}", e);
                return Err(format!("Failed to destroy Vulkan command buffers: {:?}", e));
            }
        }
        log::debug!("Vulkan command buffers destroyed");

        // Destroy Vulkan framebuffers
        if let Some(swapchain) = self.swapchain.as_ref() {
            for framebuffer in swapchain.framebuffers.iter() {
                framebuffer
                    .destroy(self.device.as_ref().unwrap().logical_device.as_ref().unwrap());
            }
            log::debug!("Vulkan framebuffers destroyed");
        }

        // Destroy Vulkan render pass
        if let Some(render_pass) = self.main_renderpass.take() {
            render_pass.destroy(device!(self));
            log::debug!("Vulkan render pass destroyed");
        }

        // Destroy Vulkan swapchain
        if self.swapchain.is_some() {
            self.swapchain().destroy()?;
            log::debug!("Vulkan swapchain destroyed");
        }

        // Destroy Vulkan device
        if let Some(mut device) = self.device.take() {
            device.destroy();
            log::debug!("Vulkan device destroyed");
        }

        // Destroy Vulkan surface
        if let Some(surface) = self.surface {
            unsafe {
                surface_loader!(self).destroy_surface(surface, None);
                log::debug!("Vulkan surface destroyed");
            }
        }

        // If this is a debug build, destroy debug messenger
        #[cfg(debug_assertions)]
        {
            if let Some(debug_callback) = self.debug_callback {
                unsafe {
                    self.debug_utils_loader
                        .as_ref()
                        .unwrap()
                        .destroy_debug_utils_messenger(debug_callback, None);
                    log::debug!("Vulkan debug callback destroyed");
                }
            }
        }

        if let Some(instance) = &self.instance {
            unsafe {
                instance.destroy_instance(None);
                log::debug!("Vulkan instance destroyed");
            }
        }
        log::debug!("Vulkan renderer shut down successfully");
        Ok(())
    }

    fn begin_frame(&mut self) -> Result<bool, String> {
        // Begin Vulkan frame rendering here
        // Check if we need to resize the swapchain, if yes, resize it and skip the frame.
        if !self.swapchain().resize_swapchain_if_needed()? {
            return Ok(false); // Skip this frame.
        }

        // Acquire the next image from the swapchain
        self.swapchain().acquire_next_image()?;

        // Begin command buffer recording
        self.commands().begin_gfx_commands_buffer()?;

        // Dynamic state
        self.renderpass().begin_main_render_pass()?;

        //TODO: --- TEMP CODE - REMOVE THIS LATER ---
        // Bind the graphics pipeline
        graphics_pipeline!(self).bind(
            device!(self),
            command_buffer!(self, self.image_index).handle,
        );

        // Bind the buffers
        unsafe {
            device!(self).cmd_bind_vertex_buffers(
                command_buffer!(self, self.image_index).handle,
                0,
                &[self.object_vertex_buffer.as_ref().unwrap().handle],
                &[0],
            );
            device!(self).cmd_bind_index_buffer(
                command_buffer!(self, self.image_index).handle,
                self.object_index_buffer.as_ref().unwrap().handle,
                0,
                vk::IndexType::UINT32,
            );

            device!(self).cmd_draw_indexed(
                command_buffer!(self, self.image_index).handle,
                3,
                1,
                0,
                0,
                0,
            );
        };
        //TODO: --- END TEMP CODE ---

        Ok(true)
    }

    fn end_frame(&mut self) -> Result<(), String> {
        // End Vulkan frame rendering here
        // End the renderpass
        self.renderpass().end_main_render_pass()?;
        self.commands().end_gfx_commands_buffer()?;

        self.fence().wait_for_previous_frame_to_complete()?;

        self.fence().update_image_sync()?;

        self.commands().submit_gfx_command_buf()?;
        self.swapchain().present()?;

        Ok(())
    }

    fn resize(&mut self, width: u32, height: u32) -> Result<(), String> {
        // Handle Vulkan swapchain resizing here
        if width == 0 || height == 0 {
            return Err("Invalid width or height".to_string());
        }

        self.cached_framebuffer_width = width;
        self.cached_framebuffer_height = height;

        // Update the framebuffer generation, this will be used to recreate the swapchain
        // when the main loop detects that the framebuffer size has changed.
        self.framebuffer_size_generation += 1;
        let current_gen = self.framebuffer_size_generation;

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
    use std::borrow::Cow;

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
