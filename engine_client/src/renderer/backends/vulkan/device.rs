use std::ffi::CStr;

use ash::vk;

use crate::device;

use super::VulkanRendererBackend;

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

pub(super) struct VulkanPhysicalDeviceSwapchainSupportInfo {
    pub capabilities: vk::SurfaceCapabilitiesKHR,
    pub formats: Vec<vk::SurfaceFormatKHR>,
    pub present_modes: Vec<vk::PresentModeKHR>,
}

pub(super) struct VulkanDevice {
    pub physical_device: ash::vk::PhysicalDevice,
    pub logical_device: Option<ash::Device>,
    pub swapchain_support: VulkanPhysicalDeviceSwapchainSupportInfo,

    pub properties: ash::vk::PhysicalDeviceProperties,
    pub features: ash::vk::PhysicalDeviceFeatures,
    pub memory: ash::vk::PhysicalDeviceMemoryProperties,

    pub graphics_queue_index: u32,
    pub compute_queue_index: u32,
    pub transfer_queue_index: u32,
    pub present_queue_index: u32,

    pub graphics_queue: ash::vk::Queue,
    pub compute_queue: ash::vk::Queue,
    pub transfer_queue: ash::vk::Queue,
    pub present_queue: ash::vk::Queue,

    pub graphics_command_pool: vk::CommandPool,

    pub depth_format: vk::Format,
}

pub(super) struct VulkanDeviceOperations<'backend> {
    backend: &'backend mut VulkanRendererBackend,
}

pub(super) trait VulkanDeviceContext {
    fn device(&mut self) -> VulkanDeviceOperations<'_>;
}

impl VulkanDeviceContext for VulkanRendererBackend {
    fn device(&mut self) -> VulkanDeviceOperations<'_> {
        VulkanDeviceOperations { backend: self }
    }
}

impl VulkanDevice {
    pub(crate) fn new(
        instance: &ash::Instance,
        surface_loader: &ash::khr::surface::Instance,
        surface: ash::vk::SurfaceKHR,
    ) -> Result<Self, String> {
        // Create a new Vulkan device using the provided context
        let mut device = match VulkanDevice::select_appropriate_physical_device(
            instance,
            surface_loader,
            surface,
        ) {
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
            instance.create_device(device.physical_device, &device_create_info, None)
        } {
            Ok(device) => Some(device),
            Err(e) => {
                return Err(format!("Failed to create Vulkan logical device: {:?}", e));
            }
        };

        // Get queues
        device.graphics_queue = unsafe {
            device
                .logical_device
                .as_ref()
                .unwrap()
                .get_device_queue(device.graphics_queue_index, 0)
        };
        device.compute_queue = unsafe {
            device
                .logical_device
                .as_ref()
                .unwrap()
                .get_device_queue(device.compute_queue_index, 0)
        };
        device.transfer_queue = unsafe {
            device
                .logical_device
                .as_ref()
                .unwrap()
                .get_device_queue(device.transfer_queue_index, 0)
        };
        device.present_queue = unsafe {
            device
                .logical_device
                .as_ref()
                .unwrap()
                .get_device_queue(device.present_queue_index, 0)
        };

        // Create command pool for graphics queue
        let command_pool_info = vk::CommandPoolCreateInfo::default()
            .queue_family_index(device.graphics_queue_index)
            .flags(vk::CommandPoolCreateFlags::RESET_COMMAND_BUFFER);

        device.graphics_command_pool = match unsafe {
            device
                .logical_device
                .as_ref()
                .unwrap()
                .create_command_pool(&command_pool_info, None)
        } {
            Ok(pool) => pool,
            Err(e) => {
                return Err(format!("Failed to create command pool: {:?}", e));
            }
        };
        log::debug!("Vulkan command pool created successfully");

        log::debug!("Vulkan logical device created successfully");

        Ok(device)
    }

    pub(crate) fn destroy(self: &mut Self) {
        // Destroy the command pool
        if let Some(logical_device) = self.logical_device.as_ref() {
            unsafe {
                logical_device.destroy_command_pool(self.graphics_command_pool, None);
                log::debug!("Vulkan command pool destroyed");
            }
        }
        // Release the queues
        self.graphics_queue = vk::Queue::null();
        self.compute_queue = vk::Queue::null();
        self.transfer_queue = vk::Queue::null();
        self.present_queue = vk::Queue::null();
        log::debug!("Vulkan queues released");

        // Destroy the Vulkan logical device
        if let Some(logical_device) = self.logical_device.take() {
            unsafe {
                logical_device.destroy_device(None);
                self.logical_device = None;
                log::debug!("Vulkan logical device destroyed");
            }
        }
    }

    pub(crate) fn query_swapchain_support(
        surface_loader: &ash::khr::surface::Instance,
        device: ash::vk::PhysicalDevice,
        surface: vk::SurfaceKHR,
    ) -> Result<VulkanPhysicalDeviceSwapchainSupportInfo, String> {
        // Query swapchain support for the vulkan device
        // and return the swapchain support info.
        let swapchain_info = VulkanPhysicalDeviceSwapchainSupportInfo {
            capabilities: match unsafe {
                surface_loader.get_physical_device_surface_capabilities(device, surface)
            } {
                Ok(capabilities) => capabilities,
                Err(e) => {
                    return Err(format!("Failed to get surface capabilities: {:?}", e));
                }
            },

            // Surface formats
            formats: match unsafe {
                surface_loader.get_physical_device_surface_formats(device, surface)
            } {
                Ok(formats) => formats,
                Err(e) => {
                    return Err(format!("Failed to get surface formats: {:?}", e));
                }
            },

            // Present modes
            present_modes: match unsafe {
                surface_loader.get_physical_device_surface_present_modes(device, surface)
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
        instance: &ash::Instance,
        surface_loader: &ash::khr::surface::Instance,
        surface: vk::SurfaceKHR,
    ) -> Result<VulkanDevice, String> {
        // Enumerate physical devices and select the appropriate one
        // based on the requirements of the application.
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
            let properties = unsafe { instance.get_physical_device_properties(device) };
            let features = unsafe { instance.get_physical_device_features(device) };
            let memory = unsafe { instance.get_physical_device_memory_properties(device) };

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
                    instance,
                    surface_loader,
                    surface,
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

                graphics_queue: vk::Queue::null(),
                compute_queue: vk::Queue::null(),
                transfer_queue: vk::Queue::null(),
                present_queue: vk::Queue::null(),

                graphics_command_pool: vk::CommandPool::null(),

                depth_format: vk::Format::UNDEFINED,
            });
            break;
        }

        if selected_device.is_none() {
            return Err("No suitable physical device found".to_string());
        }

        Ok(selected_device.unwrap())
    }

    fn check_device_support_minimal_requirements(
        instance: &ash::Instance,
        surface_loader: &ash::khr::surface::Instance,
        surface: ash::vk::SurfaceKHR,
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

        let queue_families =
            unsafe { instance.get_physical_device_queue_family_properties(device) };
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
                surface_loader.get_physical_device_surface_support(
                    device,
                    queue_info.graphics,
                    surface,
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
        let swapchain_info =
            match VulkanDevice::query_swapchain_support(surface_loader, device, surface) {
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
        let available_extensions =
            match unsafe { instance.enumerate_device_extension_properties(device) } {
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

    pub(crate) fn detect_depth_format(
        self: &Self,
        instance: &ash::Instance,
    ) -> Result<vk::Format, String> {
        // Candidate depth formats, in order of preference
        let depth_formats = [
            vk::Format::D32_SFLOAT,
            vk::Format::D32_SFLOAT_S8_UINT,
            vk::Format::D24_UNORM_S8_UINT,
        ];
        let flags = vk::FormatFeatureFlags::DEPTH_STENCIL_ATTACHMENT;

        // Select the first format that supports the required features
        // and is supported by the physical device
        let selected_format = depth_formats
            .iter()
            .filter(|&&format| {
                let format_props = unsafe {
                    instance
                        .get_physical_device_format_properties(self.physical_device, format)
                };
                if format_props.linear_tiling_features.contains(flags) {
                    return true;
                } else if format_props.optimal_tiling_features.contains(flags) {
                    return true;
                } else {
                    return false;
                }
            })
            .next();
        if let Some(format) = selected_format {
            log::info!("Selected depth format: {:?}", format);
            Ok(format.clone())
        } else {
            Err("No suitable depth format found".to_string())
        }
    }
}

impl<'backend> VulkanDeviceOperations<'backend> {
    pub fn wait_for_device_idle(&self) -> Result<(), String> {
        // Wait for the device to be idle.
        let device = device!(self.backend);
        match unsafe { device.device_wait_idle() } {
            Ok(_) => {}
            Err(err) => return Err(format!("Failed to wait for device idle: {:?}", err)),
        }

        Ok(())
    }
}
