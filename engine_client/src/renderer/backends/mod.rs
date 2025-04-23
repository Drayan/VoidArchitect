use crate::{platform::WindowHandle, renderer::RendererBackend};
use ash::{Entry, vk};
use raw_window_handle::HasDisplayHandle;

struct VulkanContext {
    instance: Option<ash::Instance>,
    physical_device: ash::vk::PhysicalDevice,
    logical_device: Option<ash::Device>,
}

pub struct RendererBackendVulkan {
    // Vulkan context
    context: Option<VulkanContext>,
}

impl RendererBackendVulkan {
    pub fn new() -> Self {
        Self { context: None }
    }
}

impl RendererBackend for RendererBackendVulkan {
    fn initialize(&mut self, window_hndl: WindowHandle) -> Result<(), String> {
        // Initialize Vulkan resources here
        unsafe {
            log::debug!("Initializing Vulkan renderer");

            let entry = Entry::linked();

            // Create Vulkan Context
            let mut context = VulkanContext {
                instance: None,
                physical_device: ash::vk::PhysicalDevice::null(),
                logical_device: None,
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

            self.context = Some(context);
            log::debug!("Vulkan renderer initialized successfully");
            Ok(())
        }
    }

    fn shutdown(&mut self) -> Result<(), String> {
        // Cleanup Vulkan resources here
        if let Some(context) = self.context.take() {
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
