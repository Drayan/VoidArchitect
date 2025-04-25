//! Vulkan backend implementation for the renderer.
//!
//! This module provides the `RendererBackendVulkan` struct, which implements the `RendererBackend`
//! trait using the Vulkan API via the `ash` crate. It manages Vulkan instance, device, surface,
//! and related resources, and provides methods for initialization, frame rendering, resizing,
//! and shutdown. Debug utilities are enabled in debug builds.

use std::borrow::Cow;

use crate::{platform::WindowHandle, renderer::RendererBackend};
use ash::{Entry, vk};
use raw_window_handle::{HasDisplayHandle, HasWindowHandle};

mod device;
mod image;
mod swapchain;
use device::VulkanDevice;

struct VulkanContext {
    framebuffer_width: u32,
    framebuffer_height: u32,

    instance: Option<ash::Instance>,
    surface: Option<ash::vk::SurfaceKHR>,
    surface_loader: Option<ash::khr::surface::Instance>,
    swapchain_loader: Option<ash::khr::swapchain::Device>,

    #[cfg(debug_assertions)]
    debug_callback: Option<ash::vk::DebugUtilsMessengerEXT>,
    #[cfg(debug_assertions)]
    debug_utils_loader: Option<ash::ext::debug_utils::Instance>,

    device: Option<VulkanDevice>,
    swapchain: Option<swapchain::VulkanSwapchain>,
    image_index: u32,
    current_frame: u32,

    recreating_swapchain: bool,
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
        let physical_device = self.device.as_ref().unwrap().physical_device;
        let mem_properties = unsafe {
            self.instance
                .as_ref()
                .unwrap()
                .get_physical_device_memory_properties(physical_device)
        };

        for i in 0..mem_properties.memory_type_count {
            if (type_filter & (1 << i)) != 0
                && mem_properties.memory_types[i as usize].property_flags.contains(properties)
            {
                return Ok(i);
            }
        }

        Err(format!(
            "Failed to find suitable memory type for filter: {} and properties: {:?}",
            type_filter, properties
        ))
    }
}

pub struct RendererBackendVulkan {
    // Vulkan context
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
}

impl RendererBackend for RendererBackendVulkan {
    fn initialize(&mut self, window_hndl: WindowHandle) -> Result<(), String> {
        // Initialize Vulkan resources here
        if window_hndl.window.display_handle().is_err() {
            return Err("Window handle is invalid".to_string());
        }
        if window_hndl.window.window_handle().is_err() {
            return Err("Window handle is invalid".to_string());
        }

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
                framebuffer_width: window_hndl.window.inner_size().width,
                framebuffer_height: window_hndl.window.inner_size().height,
                swapchain: None,
                recreating_swapchain: false,
            };

            // Create Vulkan Application Info
            let app_infos = vk::ApplicationInfo::default()
                .application_name(c"Void Architect")
                .engine_name(c"Void Architect Engine")
                .application_version(0)
                .engine_version(0)
                .api_version(vk::make_api_version(0, 1, 0, 0));

            let layer_names = vec![c"VK_LAYER_KHRONOS_validation".as_ptr()];

            let mut extension_names = ash_window::enumerate_required_extensions(
                window_hndl.window.display_handle().unwrap().as_raw(),
            )
            .unwrap()
            .to_vec();

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
            let surface = ash_window::create_surface(
                &entry,
                &context.instance.as_ref().unwrap(),
                window_hndl.window.display_handle().unwrap().as_raw(),
                window_hndl.window.window_handle().unwrap().as_raw(),
                None,
            );
            context.surface = match surface {
                Ok(surface) => Some(surface),
                Err(e) => {
                    log::error!("Failed to create Vulkan surface: {:?}", e);
                    return Err(format!("Failed to create Vulkan surface: {:?}", e));
                }
            };
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

            self.context = Some(context);
            log::debug!("Vulkan renderer initialized successfully");
            Ok(())
        }
    }

    fn shutdown(&mut self) -> Result<(), String> {
        // Cleanup Vulkan resources here
        if let Some(mut context) = self.context.take() {
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
        Ok(())
    }

    fn end_frame(&mut self) -> Result<(), String> {
        // End Vulkan frame rendering here
        Ok(())
    }

    fn resize(&mut self, _width: u32, _height: u32) -> Result<(), String> {
        // Handle Vulkan swapchain resizing here
        log::debug!("Resizing Vulkan swapchain");
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
