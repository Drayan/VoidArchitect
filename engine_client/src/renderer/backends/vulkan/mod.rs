//! Vulkan backend implementation for the renderer.
//!
//! This module provides the `RendererBackendVulkan` struct, which implements the `RendererBackend`
//! trait using the Vulkan API via the `ash` crate. It manages Vulkan instance, device, surface,
//! and related resources, and provides methods for initialization, frame rendering, resizing,
//! and shutdown. Debug utilities are enabled in debug builds.

use std::borrow::Cow;

use crate::{platform::WindowHandle, renderer::RendererBackend};
use ash::{
    Entry,
    vk::{self, Handle},
};
use commands::VulkanCommandBuffer;
use framebuffer::VulkanFramebuffer;
//use raw_window_handle::{HasDisplayHandle, HasWindowHandle};

pub mod buffer;
pub mod commands;
mod device;
mod framebuffer;
mod image;
mod pipeline;
mod renderpass;
mod swapchain;

use buffer::{BufferType, Vertex, VulkanBuffer};
use device::VulkanDevice;
use pipeline::VulkanPipeline;
use raw_window_handle::{HasDisplayHandle, HasWindowHandle};
use renderpass::VulkanRenderPass;
use swapchain::VulkanSwapchain;

struct VulkanContext {
    // Current framebuffer width and height.
    framebuffer_width: u32,
    framebuffer_height: u32,

    // Vulkan instance and surface (None if not initialized).
    instance: Option<ash::Instance>,
    surface: Option<ash::vk::SurfaceKHR>,

    // Vulkan loader handles for surface and swapchain extensions.
    surface_loader: Option<ash::khr::surface::Instance>,
    swapchain_loader: Option<ash::khr::swapchain::Device>,

    // Debug utilities (only in debug builds).
    #[cfg(debug_assertions)]
    debug_callback: Option<ash::vk::DebugUtilsMessengerEXT>,
    #[cfg(debug_assertions)]
    debug_utils_loader: Option<ash::ext::debug_utils::Instance>,

    // Logical device and swapchain state.
    device: Option<VulkanDevice>,
    swapchain: Option<swapchain::VulkanSwapchain>,
    main_renderpass: Option<VulkanRenderPass>,

    // Graphics pipeline for rendering.
    graphics_pipeline: Option<pipeline::VulkanPipeline>,

    // Vertex buffer for triangle data
    vertex_buffer: Option<VulkanBuffer>,

    // Command buffers for graphics operations.
    graphics_cmds_buffers: Vec<VulkanCommandBuffer>,

    // Current image index and frame number.
    image_index: u32,
    current_frame: u32,

    // Flag indicating if swapchain is being recreated.
    recreating_swapchain: bool,

    // Maximum number of frames in flight
    max_frames_in_flight: usize,

    // Synchronization primitives
    image_available_semaphores: Vec<ash::vk::Semaphore>,
    render_finished_semaphores: Vec<ash::vk::Semaphore>,
    in_flight_fences: Vec<ash::vk::Fence>,
}

impl VulkanContext {
    /// Finds a suitable memory type for the given memory requirements and flags.
    ///
    /// # Arguments
    /// * `type_filter` - The memory type filter.
    /// * `properties` - The desired memory properties.
    ///
    /// # Returns
    /// * `u32` - The index of the suitable memory type.
    pub fn find_memory_type(
        &self,
        type_filter: u32,
        properties: vk::MemoryPropertyFlags,
    ) -> Result<u32, String> {
        // # Reason: Vulkan requires selecting a memory type that matches both a bitmask (type_filter)
        // and required property flags. This iterates all memory types and returns the first match.
        let physical_device = self.device.as_ref().unwrap().physical_device;
        let mem_properties = unsafe {
            self.instance
                .as_ref()
                .unwrap()
                .get_physical_device_memory_properties(physical_device)
        };

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

pub struct RendererBackendVulkan {
    // Vulkan context
    // Holds the Vulkan context state for this backend.
    context: Option<VulkanContext>,
}

impl RendererBackendVulkan {
    /// Creates a new Vulkan renderer backend instance.
    ///
    /// # Returns
    /// * `RendererBackendVulkan` - A new Vulkan backend instance.
    pub fn new() -> Self {
        Self { context: None }
    }

    fn create_commands_buffers(&mut self) -> Result<(), String> {
        // If the context already has command buffers, destroy them
        if let Some(context) = &mut self.context {
            // Extract needed values first to avoid multiple borrows
            let logical_device =
                context.device.as_ref().unwrap().logical_device.as_ref().unwrap();
            let command_pool = context.device.as_ref().unwrap().graphics_command_pool;

            for buffer in context.graphics_cmds_buffers.iter_mut() {
                buffer.destroy(logical_device, command_pool);
            }
            context.graphics_cmds_buffers.clear();
        }
        // Create new command buffers
        if let Some(context) = &mut self.context {
            // Extract all needed values first
            let swapchain = context.swapchain.as_ref().unwrap();
            let device = context.device.as_ref().unwrap();
            let logical_device = device.logical_device.as_ref().unwrap();
            let command_pool = device.graphics_command_pool;
            let image_count = swapchain.images.len();
            let renderpass = context.main_renderpass.as_ref().unwrap();
            let pipeline = context.graphics_pipeline.as_ref().unwrap();
            let vertex_buffer = context.vertex_buffer.as_ref().unwrap();
            let framebuffers = &swapchain.framebuffers;
            let width = context.framebuffer_width;
            let height = context.framebuffer_height;

            // Now create each buffer with the extracted values
            for (i, framebuffer) in framebuffers.iter().enumerate() {
                // Create command buffer
                let buffer = VulkanCommandBuffer::new(logical_device, command_pool, true)?;

                // Begin command buffer recording
                let begin_info = vk::CommandBufferBeginInfo::default()
                    .flags(vk::CommandBufferUsageFlags::SIMULTANEOUS_USE);

                unsafe {
                    logical_device
                        .begin_command_buffer(buffer.handle, &begin_info)
                        .map_err(|e| format!("Failed to begin command buffer: {}", e))?;
                }

                // Begin render pass
                let clear_values = [
                    vk::ClearValue {
                        color: vk::ClearColorValue {
                            float32: [0.0, 0.0, 0.0, 1.0],
                        },
                    },
                    vk::ClearValue {
                        depth_stencil: vk::ClearDepthStencilValue {
                            depth: 1.0,
                            stencil: 0,
                        },
                    },
                ];

                let render_pass_begin_info = vk::RenderPassBeginInfo::default()
                    .render_pass(renderpass.handle)
                    .framebuffer(framebuffer.handle)
                    .render_area(vk::Rect2D {
                        offset: vk::Offset2D { x: 0, y: 0 },
                        extent: vk::Extent2D { width, height },
                    })
                    .clear_values(&clear_values);

                unsafe {
                    logical_device.cmd_begin_render_pass(
                        buffer.handle,
                        &render_pass_begin_info,
                        vk::SubpassContents::INLINE,
                    );

                    // Bind the graphics pipeline
                    logical_device.cmd_bind_pipeline(
                        buffer.handle,
                        vk::PipelineBindPoint::GRAPHICS,
                        pipeline.pipeline,
                    );

                    // Bind the vertex buffer
                    let vertex_buffers = [vertex_buffer.buffer];
                    let offsets = [0];
                    logical_device.cmd_bind_vertex_buffers(
                        buffer.handle,
                        0,
                        &vertex_buffers,
                        &offsets,
                    );

                    // Draw 3 vertices (one triangle)
                    logical_device.cmd_draw(buffer.handle, 3, 1, 0, 0);

                    // End render pass
                    logical_device.cmd_end_render_pass(buffer.handle);

                    // End command buffer recording
                    logical_device
                        .end_command_buffer(buffer.handle)
                        .map_err(|e| format!("Failed to end command buffer: {}", e))?;
                }

                context.graphics_cmds_buffers.push(buffer);
            }
        }

        Ok(())
    }

    fn destroy_commands_buffers(&mut self, context: &mut VulkanContext) -> Result<(), String> {
        // Destroy Vulkan command buffers here
        // Extract needed values first to avoid multiple borrows
        let logical_device = context.device.as_ref().unwrap().logical_device.as_ref().unwrap();
        let command_pool = context.device.as_ref().unwrap().graphics_command_pool;

        // Now destroy each buffer with the extracted values
        for buffer in context.graphics_cmds_buffers.iter_mut() {
            buffer.destroy(logical_device, command_pool);
        }
        context.graphics_cmds_buffers.clear();
        Ok(())
    }

    fn recreate_framebuffers(
        &self,
        device: &ash::Device,
        swapchain: &mut VulkanSwapchain,
        renderpass: VulkanRenderPass,
        width: u32,
        height: u32,
    ) -> Result<(), String> {
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

        unsafe {
            log::debug!("Initializing Vulkan renderer");

            let entry = Entry::linked();

            // Create Vulkan Context
            let mut context = VulkanContext {
                instance: None,
                surface: None,
                surface_loader: None,
                swapchain_loader: None,

                #[cfg(debug_assertions)]
                debug_callback: None,
                #[cfg(debug_assertions)]
                debug_utils_loader: None,

                device: None,

                current_frame: 0,
                image_index: 0,
                framebuffer_width: width,
                framebuffer_height: height,

                swapchain: None,
                main_renderpass: None,
                graphics_pipeline: None,
                vertex_buffer: None,

                graphics_cmds_buffers: vec![],

                recreating_swapchain: false,
                max_frames_in_flight: 2, // Double buffering
                image_available_semaphores: Vec::new(),
                render_finished_semaphores: Vec::new(),
                in_flight_fences: Vec::new(),
            };

            let max_frames_in_flight = context.max_frames_in_flight;
            let device = context.device.as_ref().unwrap().logical_device.as_ref().unwrap();

            let semaphore_info = vk::SemaphoreCreateInfo::default();
            let fence_info =
                vk::FenceCreateInfo::default().flags(vk::FenceCreateFlags::SIGNALED);

            for _ in 0..max_frames_in_flight {
                let image_available_semaphore = unsafe {
                    device.create_semaphore(&semaphore_info, None).map_err(|e| {
                        format!("Failed to create image available semaphore: {}", e)
                    })?
                };
                context.image_available_semaphores.push(image_available_semaphore);

                let render_finished_semaphore = unsafe {
                    device.create_semaphore(&semaphore_info, None).map_err(|e| {
                        format!("Failed to create render finished semaphore: {}", e)
                    })?
                };
                context.render_finished_semaphores.push(render_finished_semaphore);

                let in_flight_fence = unsafe {
                    device
                        .create_fence(&fence_info, None)
                        .map_err(|e| format!("Failed to create in flight fence: {}", e))?
                };
                context.in_flight_fences.push(in_flight_fence);
            }

            // Create Vulkan Application Info
            let app_infos = vk::ApplicationInfo::default()
                .application_name(c"Void Architect")
                .engine_name(c"Void Architect Engine")
                .application_version(0)
                .engine_version(0)
                .api_version(vk::make_api_version(0, 1, 0, 0));

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
            context.device = match VulkanDevice::new(&context) {
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
            let width = context.framebuffer_width;
            let height = context.framebuffer_height;
            context.swapchain =
                match swapchain::VulkanSwapchain::new(&mut context, width, height) {
                    Ok(swapchain) => Some(swapchain),
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
            let device = context.device.as_ref().unwrap().logical_device.as_ref().unwrap();
            let swapchain = context.swapchain.as_mut().unwrap();
            let renderpass = context.main_renderpass.clone().unwrap();

            match self.recreate_framebuffers(
                device,
                swapchain,
                renderpass.clone(),
                width,
                height,
            ) {
                Ok(_) => {}
                Err(e) => {
                    log::error!("Failed to create Vulkan framebuffers: {:?}", e);
                    return Err(format!("Failed to create Vulkan framebuffers: {:?}", e));
                }
            };

            // Create graphics pipeline
            let shader_dir = std::path::Path::new("assets/shaders");
            let extent = vk::Extent2D {
                width: context.framebuffer_width,
                height: context.framebuffer_height,
            };
            context.graphics_pipeline = Some(pipeline::create_graphics_pipeline(
                device,
                renderpass.handle,
                extent,
                shader_dir,
            ));
            log::debug!("Vulkan graphics pipeline created successfully");

            // Create vertex buffer with triangle data
            let vertices = [
                Vertex::new([-0.5, -0.5, 0.0], [1.0, 0.0, 0.0]), // Bottom left (red)
                Vertex::new([0.5, -0.5, 0.0], [0.0, 1.0, 0.0]),  // Bottom right (green)
                Vertex::new([0.0, 0.5, 0.0], [0.0, 0.0, 1.0]),   // Top (blue)
            ];

            // Create vertex buffer
            context.vertex_buffer = match VulkanBuffer::create_vertex_buffer(
                device,
                &vertices,
                |type_filter, properties| context.find_memory_type(type_filter, properties),
            ) {
                Ok(buffer) => {
                    log::debug!("Vulkan vertex buffer created successfully");
                    Some(buffer)
                }
                Err(e) => {
                    log::error!("Failed to create Vulkan vertex buffer: {:?}", e);
                    return Err(format!("Failed to create Vulkan vertex buffer: {:?}", e));
                }
            };

            // Create command buffers
            match self.create_commands_buffers() {
                Ok(_) => {}
                Err(e) => {
                    log::error!("Failed to create Vulkan command buffers: {:?}", e);
                    return Err(format!("Failed to create Vulkan command buffers: {:?}", e));
                }
            };
            log::debug!("Vulkan command buffers created successfully");

            self.context = Some(context);
            log::debug!("Vulkan renderer initialized successfully");
            Ok(())
        }
    }

    fn shutdown(&mut self) -> Result<(), String> {
        // Cleanup Vulkan resources here
        if let Some(mut context) = self.context.take() {
            log::debug!("Shutting down Vulkan renderer");

            let device = context.device.as_ref().unwrap().logical_device.as_ref().unwrap();

            for i in 0..context.max_frames_in_flight {
                unsafe {
                    device.destroy_semaphore(context.image_available_semaphores[i], None);
                    device.destroy_semaphore(context.render_finished_semaphores[i], None);
                    device.destroy_fence(context.in_flight_fences[i], None);
                }
            }

            // Destroy Vulkan command buffers
            match self.destroy_commands_buffers(&mut context) {
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

            // Destroy Vulkan vertex buffer
            if let Some(vertex_buffer) = context.vertex_buffer.take() {
                let device = context.device.as_ref().unwrap().logical_device.as_ref().unwrap();
                vertex_buffer.destroy(device);
                log::debug!("Vulkan vertex buffer destroyed");
            }

            // Destroy Vulkan graphics pipeline
            if let Some(pipeline) = context.graphics_pipeline.take() {
                unsafe {
                    let device =
                        context.device.as_ref().unwrap().logical_device.as_ref().unwrap();
                    device.destroy_pipeline(pipeline.pipeline, None);
                    device.destroy_pipeline_layout(pipeline.layout, None);
                }
                log::debug!("Vulkan graphics pipeline destroyed");
            }

            // Destroy Vulkan render pass
            if let Some(render_pass) = context.main_renderpass.take() {
                render_pass.destroy(&context);
                log::debug!("Vulkan render pass destroyed");
            }

            // Destroy Vulkan swapchain
            if let Some(mut swapchain) = context.swapchain.take() {
                swapchain.destroy(&context);
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

    fn begin_frame(&mut self) -> Result<(), String> {
        // Begin Vulkan frame rendering here
        if self.context.is_none() {
            return Err("Vulkan context is not initialized".to_string());
        }

        Ok(())
    }

    fn end_frame(&mut self) -> Result<(), String> {
        // End Vulkan frame rendering here
        if self.context.is_none() {
            return Err("Vulkan context is not initialized".to_string());
        }
        Ok(())
    }

    fn resize(&mut self, width: u32, height: u32) -> Result<(), String> {
        // Handle Vulkan swapchain resizing here
        // log::debug!("Resizing...");
        // let context = self.context.as_mut().unwrap();
        // let mut swapchain = context.swapchain.take().unwrap();
        // swapchain.recreate(context, width, height)?;
        // context.swapchain = Some(swapchain);
        // context.framebuffer_width = width;
        // context.framebuffer_height = height;
        // log::debug!("Resized to {}x{}", width, height);
        if width == 0 || height == 0 {
            return Err("Invalid width or height".to_string());
        }

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
