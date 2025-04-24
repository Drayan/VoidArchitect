//! Vulkan backend implementation for the renderer.
//!
//! This module provides the `RendererBackendVulkan` struct, which implements the `RendererBackend`
//! trait using the Vulkan API via the `ash` crate. It manages Vulkan instance, device, surface,
//! and related resources, and provides methods for initialization, frame rendering, resizing,
//! and shutdown. Debug utilities are enabled in debug builds.

use std::{borrow::Cow, ffi::CStr};

use crate::{platform::WindowHandle, renderer::RendererBackend};
use ash::{Entry, vk};
use raw_window_handle::{HasDisplayHandle, HasWindowHandle};

struct VulkanPhysicalDeviceRequirements {
    graphics: bool,
    compute: bool,
    transfer: bool,
    present: bool,

    sampler_anisotropy: bool,
    discrete_gpu: bool,

    extensions_names: Vec<&'static CStr>,
}

struct VulkanPhysicalDeviceQueueFamilyInfo {
    graphics: u32,
    compute: u32,
    transfer: u32,
    present: u32,
}

struct VulkanPhysicalDeviceSwapchainSupportInfo {
    capabilities: vk::SurfaceCapabilitiesKHR,
    formats: Vec<vk::SurfaceFormatKHR>,
    present_modes: Vec<vk::PresentModeKHR>,
}

struct VulkanDevice {
    physical_device: ash::vk::PhysicalDevice,
    logical_device: Option<ash::Device>,
    swapchain_support: VulkanPhysicalDeviceSwapchainSupportInfo,

    properties: ash::vk::PhysicalDeviceProperties,
    features: ash::vk::PhysicalDeviceFeatures,
    memory: ash::vk::PhysicalDeviceMemoryProperties,

    graphics_queue_index: u32,
    compute_queue_index: u32,
    transfer_queue_index: u32,
    present_queue_index: u32,
}

impl VulkanDevice {
    fn new(context: &VulkanContext) -> Result<Self, String> {
        // Create a new Vulkan device using the provided context
        let mut device = match VulkanDevice::select_appropriate_physical_device(context) {
            Ok(device) => device,
            Err(e) => {
                return Err(format!("Failed to create Vulkan device: {:?}", e));
            }
        };

        // Create logical device
        // NOTE: Do not create additionnal queues for shared indices.
        let present_share_graphics_queue =
            device.graphics_queue_index == device.present_queue_index;
        let transfer_share_graphics_queue =
            device.graphics_queue_index == device.transfer_queue_index;
        let compute_share_graphics_queue =
            device.graphics_queue_index == device.compute_queue_index;
        // Collect unique queue family indices, preserving order: graphics, present, transfer
        let mut indices = vec![device.graphics_queue_index];
        if !present_share_graphics_queue
            && device.present_queue_index != device.graphics_queue_index
        {
            indices.push(device.present_queue_index);
        }
        if !transfer_share_graphics_queue
            && device.transfer_queue_index != device.graphics_queue_index
            && device.transfer_queue_index != device.present_queue_index
        {
            indices.push(device.transfer_queue_index);
        }
        if !compute_share_graphics_queue
            && device.compute_queue_index != device.graphics_queue_index
            && device.compute_queue_index != device.present_queue_index
            && device.compute_queue_index != device.transfer_queue_index
        {
            indices.push(device.compute_queue_index);
        }

        // For each unique queue family, create a DeviceQueueCreateInfo
        let queue_priority = [1.0f32];
        let queue_create_infos: Vec<vk::DeviceQueueCreateInfo> = indices
            .iter()
            .map(|&queue_family_index| {
                let queue_count = if queue_family_index == device.graphics_queue_index {
                    1
                } else {
                    1
                };
                let mut create_info = vk::DeviceQueueCreateInfo::default()
                    .queue_family_index(queue_family_index)
                    .queue_priorities(&queue_priority);
                create_info.queue_count = queue_count;
                create_info
            })
            .collect();

        // Request device features
        //TODO: Should be configuration driven
        let mut device_features = vk::PhysicalDeviceFeatures::default();
        device_features.sampler_anisotropy = vk::TRUE; // Request anisotropy

        // Create logical device
        let extensions_names = [
            ash::khr::swapchain::NAME.as_ptr(),
            #[cfg(any(target_os = "macos", target_os = "ios"))]
            ash::khr::portability_subset::NAME.as_ptr(),
        ];
        let device_create_info = vk::DeviceCreateInfo::default()
            .queue_create_infos(&queue_create_infos)
            .enabled_features(&device_features)
            .enabled_extension_names(&extensions_names);

        device.logical_device = match unsafe {
            context.instance.as_ref().unwrap().create_device(
                device.physical_device,
                &device_create_info,
                None,
            )
        } {
            Ok(device) => Some(device),
            Err(e) => {
                return Err(format!("Failed to create Vulkan logical device: {:?}", e));
            }
        };

        Ok(device)
    }

    fn destroy(self: &mut Self) {
        // Destroy the Vulkan device
        if let Some(logical_device) = self.logical_device.take() {
            unsafe {
                logical_device.destroy_device(None);
                self.logical_device = None;
                log::debug!("Vulkan logical device destroyed");
            }
        }
    }

    fn query_swapchain_support(
        context: &VulkanContext,
        device: ash::vk::PhysicalDevice,
        surface: vk::SurfaceKHR,
    ) -> Result<VulkanPhysicalDeviceSwapchainSupportInfo, String> {
        // Query swapchain support for the vulkan device
        // and return the swapchain support info.
        let swapchain_info = VulkanPhysicalDeviceSwapchainSupportInfo {
            capabilities: match unsafe {
                context
                    .surface_loader
                    .as_ref()
                    .unwrap()
                    .get_physical_device_surface_capabilities(device, surface)
            } {
                Ok(capabilities) => capabilities,
                Err(e) => {
                    return Err(format!("Failed to get surface capabilities: {:?}", e));
                }
            },

            // Surface formats
            formats: match unsafe {
                context
                    .surface_loader
                    .as_ref()
                    .unwrap()
                    .get_physical_device_surface_formats(device, surface)
            } {
                Ok(formats) => formats,
                Err(e) => {
                    return Err(format!("Failed to get surface formats: {:?}", e));
                }
            },

            // Present modes
            present_modes: match unsafe {
                context
                    .surface_loader
                    .as_ref()
                    .unwrap()
                    .get_physical_device_surface_present_modes(device, surface)
            } {
                Ok(present_modes) => present_modes,
                Err(e) => {
                    return Err(format!("Failed to get surface present modes: {:?}", e));
                }
            },
        };

        Ok(swapchain_info)
    }

    fn select_appropriate_physical_device(
        context: &VulkanContext,
    ) -> Result<VulkanDevice, String> {
        // Enumerate physical devices and select the appropriate one
        // based on the requirements of the application.
        let instance = context.instance.as_ref().expect("Vulkan instance not initialized");
        let physical_devices = match unsafe { instance.enumerate_physical_devices() } {
            Ok(devices) => devices,
            Err(e) => {
                return Err(format!("Failed to enumerate physical devices: {:?}", e));
            }
        };

        if physical_devices.is_empty() {
            return Err("No physical devices found".to_string());
        }

        // Iterate through the physical devices and check their properties
        // and features to find a suitable device.
        let mut selected_device: Option<VulkanDevice> = None;
        for device in physical_devices {
            let properties = unsafe {
                context.instance.as_ref().unwrap().get_physical_device_properties(device)
            };
            let features = unsafe {
                context.instance.as_ref().unwrap().get_physical_device_features(device)
            };
            let memory = unsafe {
                context
                    .instance
                    .as_ref()
                    .unwrap()
                    .get_physical_device_memory_properties(device)
            };

            //TODO: These requirements should be passed from the application
            let requirements = VulkanPhysicalDeviceRequirements {
                graphics: true,
                compute: true,
                transfer: true,
                present: true,

                sampler_anisotropy: true,
                discrete_gpu: false, // Macbook Pro M1 2020 is not a discrete GPU

                extensions_names: vec![ash::khr::swapchain::NAME],
            };
            let (queue_info, swapchain_info) =
                match VulkanDevice::check_device_support_minimal_requirements(
                    context,
                    device,
                    properties,
                    features,
                    memory,
                    requirements,
                ) {
                    Ok(info) => info,
                    Err(e) => {
                        log::error!("Failed to check device support: {:?}", e);
                        continue;
                    }
                };

            log::info!(
                "Selected device: {}",
                properties.device_name.iter().map(|&c| c as u8 as char).collect::<String>()
            );
            log::info!(
                "GPU Driver Version: {}.{}.{}",
                vk::api_version_major(properties.driver_version),
                vk::api_version_minor(properties.driver_version),
                vk::api_version_patch(properties.driver_version)
            );
            log::info!(
                "Vulkan API Version: {}.{}.{}",
                vk::api_version_major(properties.api_version),
                vk::api_version_minor(properties.api_version),
                vk::api_version_patch(properties.api_version)
            );

            // Memory information
            for memory_heap_index in 0..memory.memory_heap_count {
                let memory_heap = memory.memory_heaps[memory_heap_index as usize];
                let size_gib = memory_heap.size / (1024 * 1024 * 1024);
                match memory_heap.flags {
                    vk::MemoryHeapFlags::DEVICE_LOCAL => {
                        log::info!("GPU Local Memory: {:2} GiB", size_gib);
                    }
                    _ => {
                        log::info!("Shared System Memory: {:2} GiB", size_gib);
                    }
                }
            }

            // Store the selected device
            selected_device = Some(VulkanDevice {
                physical_device: device,
                logical_device: None,
                swapchain_support: swapchain_info,

                properties,
                features,
                memory,

                graphics_queue_index: queue_info.graphics,
                compute_queue_index: queue_info.compute,
                transfer_queue_index: queue_info.transfer,
                present_queue_index: queue_info.present,
            });
            break;
        }

        if selected_device.is_none() {
            return Err("No suitable physical device found".to_string());
        }

        Ok(selected_device.unwrap())
    }

    fn check_device_support_minimal_requirements(
        context: &VulkanContext,
        device: ash::vk::PhysicalDevice,
        properties: ash::vk::PhysicalDeviceProperties,
        features: ash::vk::PhysicalDeviceFeatures,
        _memory: ash::vk::PhysicalDeviceMemoryProperties,
        requirements: VulkanPhysicalDeviceRequirements,
    ) -> Result<
        (
            VulkanPhysicalDeviceQueueFamilyInfo,
            VulkanPhysicalDeviceSwapchainSupportInfo,
        ),
        String,
    > {
        let mut queue_info = VulkanPhysicalDeviceQueueFamilyInfo {
            graphics: u32::MAX,
            compute: u32::MAX,
            transfer: u32::MAX,
            present: u32::MAX,
        };

        // Check if the device supports the required features
        if requirements.discrete_gpu
            && properties.device_type != vk::PhysicalDeviceType::DISCRETE_GPU
        {
            return Err("Device is not a discrete GPU".to_string());
        }

        let queue_families = unsafe {
            context
                .instance
                .as_ref()
                .unwrap()
                .get_physical_device_queue_family_properties(device)
        };
        log::info!(" Graphics | Present  |  Compute | Transfer | Name");
        let mut min_transfer_score = i32::MAX;
        for family in queue_families {
            let mut current_transfer_score = 0;

            // Graphics ?
            if family.queue_flags.contains(vk::QueueFlags::GRAPHICS) {
                queue_info.graphics = family.queue_count;
                current_transfer_score += 1;
            }

            // Compute ?
            if family.queue_flags.contains(vk::QueueFlags::COMPUTE) {
                queue_info.compute = family.queue_count;
                current_transfer_score += 1;
            }

            // Transfer ?
            if family.queue_flags.contains(vk::QueueFlags::TRANSFER) {
                if current_transfer_score < min_transfer_score {
                    queue_info.transfer = family.queue_count;
                    min_transfer_score = current_transfer_score;
                }
            }

            // Present ?
            let support_present = match unsafe {
                context.surface_loader.as_ref().unwrap().get_physical_device_surface_support(
                    device,
                    queue_info.graphics,
                    context.surface.unwrap(),
                )
            } {
                Ok(support) => support,
                Err(e) => {
                    return Err(format!("Failed to check surface support: {:?}", e));
                }
            };
            if support_present {
                queue_info.present = family.queue_count;
            }

            // Print queue family info
            log::info!(
                "{:10}|{:10}|{:10}|{:10}| {}",
                if queue_info.graphics != u32::MAX {
                    "Yes"
                } else {
                    "No"
                },
                if queue_info.present != u32::MAX {
                    "Yes"
                } else {
                    "No"
                },
                if queue_info.compute != u32::MAX {
                    "Yes"
                } else {
                    "No"
                },
                if queue_info.transfer != u32::MAX {
                    "Yes"
                } else {
                    "No"
                },
                properties.device_name.iter().map(|&c| c as u8 as char).collect::<String>()
            );
        }

        if (!requirements.graphics
            || (requirements.graphics && queue_info.graphics != u32::MAX))
            && (!requirements.compute
                || (requirements.compute && queue_info.compute != u32::MAX))
            && (!requirements.transfer
                || (requirements.transfer && queue_info.transfer != u32::MAX))
            && (!requirements.present
                || (requirements.present && queue_info.present != u32::MAX))
        {
            log::info!("Device supports the required features");
            log::debug!("Graphics Family Index: {}", queue_info.graphics);
            log::debug!("Compute Family Index: {}", queue_info.compute);
            log::debug!("Transfer Family Index: {}", queue_info.transfer);
            log::debug!("Present Family Index: {}", queue_info.present);
        } else {
            return Err("Device does not support the required features".to_string());
        }

        // Query swapchain infos
        let swapchain_info = match VulkanDevice::query_swapchain_support(
            context,
            device,
            context.surface.unwrap(),
        ) {
            Ok(info) => info,
            Err(e) => {
                return Err(format!("Failed to query swapchain support: {:?}", e));
            }
        };

        // Validate swapchain support
        if swapchain_info.formats.len() < 1 || swapchain_info.present_modes.len() < 1 {
            return Err(
                "Swapchain does not support the required formats or present modes".to_string(),
            );
        }

        // Check if the device supports the required extensions
        let available_extensions = match unsafe {
            context.instance.as_ref().unwrap().enumerate_device_extension_properties(device)
        } {
            Ok(extensions) => extensions,
            Err(e) => {
                return Err(format!("Failed to enumerate device extensions: {:?}", e));
            }
        };

        for required_extension in &requirements.extensions_names {
            let mut found = false;
            for available_extension in &available_extensions {
                if *required_extension
                    == available_extension.extension_name_as_c_str().unwrap()
                {
                    found = true;
                    break;
                }
            }
            if !found {
                return Err(format!(
                    "Device does not support required extension: {}",
                    required_extension.to_str().unwrap()
                ));
            }
        }

        // Sampler anisotropy
        if requirements.sampler_anisotropy && features.sampler_anisotropy == vk::FALSE {
            return Err("Device does not support sampler anisotropy".to_string());
        }

        Ok((queue_info, swapchain_info))
    }
}

struct VulkanContext {
    instance: Option<ash::Instance>,
    surface: Option<ash::vk::SurfaceKHR>,
    surface_loader: Option<ash::khr::surface::Instance>,

    #[cfg(debug_assertions)]
    debug_callback: Option<ash::vk::DebugUtilsMessengerEXT>,
    #[cfg(debug_assertions)]
    debug_utils_loader: Option<ash::ext::debug_utils::Instance>,

    vulkan_device: Option<VulkanDevice>,
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

                #[cfg(debug_assertions)]
                debug_callback: None,
                #[cfg(debug_assertions)]
                debug_utils_loader: None,

                vulkan_device: None,
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
            context.vulkan_device = match VulkanDevice::new(&context) {
                Ok(device) => Some(device),
                Err(e) => {
                    log::error!("Failed to create Vulkan device: {:?}", e);
                    return Err(format!("Failed to create Vulkan device: {:?}", e));
                }
            };
            log::debug!("Vulkan device created successfully");

            self.context = Some(context);
            log::debug!("Vulkan renderer initialized successfully");
            Ok(())
        }
    }

    fn shutdown(&mut self) -> Result<(), String> {
        // Cleanup Vulkan resources here
        if let Some(mut context) = self.context.take() {
            // Destroy Vulkan device
            if let Some(mut device) = context.vulkan_device.take() {
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
