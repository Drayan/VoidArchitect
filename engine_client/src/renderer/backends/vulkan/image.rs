use ash::vk;

use super::VulkanContext;

pub(super) struct VulkanImage {
    handle: vk::Image,
    memory: vk::DeviceMemory,
    pub view: vk::ImageView,
    width: u32,
    height: u32,
}

impl VulkanImage {
    pub fn new(
        device: &ash::Device,
        physical_device: ash::vk::PhysicalDevice,
        instance: &ash::Instance,
        width: u32,
        height: u32,
        format: vk::Format,
        tiling: vk::ImageTiling,
        usage: vk::ImageUsageFlags,
        memory_flags: vk::MemoryPropertyFlags,
        create_view: bool,
        view_aspect_flags: vk::ImageAspectFlags,
    ) -> Result<Self, String> {
        let image_create_info = vk::ImageCreateInfo {
            image_type: vk::ImageType::TYPE_2D, //TODO: Support 3D images
            extent: vk::Extent3D {
                width,
                height,
                depth: 1, //TODO: Support configurable depth
            },
            mip_levels: 4,   //TODO: Support mip levels
            array_layers: 1, //TODO: Support array layers
            format,
            tiling,
            initial_layout: vk::ImageLayout::UNDEFINED,
            usage,
            sharing_mode: vk::SharingMode::EXCLUSIVE, //TODO: Support concurrent sharing
            samples: vk::SampleCountFlags::TYPE_1,    //TODO: Support sample count
            flags: vk::ImageCreateFlags::empty(),
            ..Default::default()
        };

        let handle = match unsafe { device.create_image(&image_create_info, None) } {
            Ok(handle) => handle,
            Err(err) => return Err(format!("Failed to create image: {}", err)),
        };

        // Query memory requirements
        let memory_requirements = unsafe { device.get_image_memory_requirements(handle) };
        let memory_type_index = match VulkanContext::find_memory_type(
            physical_device,
            instance,
            memory_requirements.memory_type_bits,
            memory_flags,
        ) {
            Ok(index) => index,
            Err(e) => {
                unsafe { device.destroy_image(handle, None) };
                return Err(format!(
                    "Failed to find suitable memory type for image: {}",
                    e
                ));
            }
        };

        // Allocate memory on the device
        let allocate_info = vk::MemoryAllocateInfo {
            allocation_size: memory_requirements.size,
            memory_type_index,
            ..Default::default()
        };
        let memory = match unsafe { device.allocate_memory(&allocate_info, None) } {
            Ok(memory) => memory,
            Err(err) => {
                unsafe { device.destroy_image(handle, None) };
                return Err(format!("Failed to allocate image memory: {}", err));
            }
        };

        // Bind memory to the image
        //TODO: Configurable offset
        match unsafe { device.bind_image_memory(handle, memory, 0) } {
            Ok(_) => {}
            Err(err) => {
                unsafe {
                    device.free_memory(memory, None);
                    device.destroy_image(handle, None);
                }
                return Err(format!("Failed to bind image memory: {}", err));
            }
        };

        let image = Self {
            handle,
            memory,
            view: vk::ImageView::null(),
            width,
            height,
        };

        // Create image view if requested
        if create_view {
            match image.create_view_for(device, format, view_aspect_flags) {
                Ok(view) => {
                    return Ok(Self {
                        handle,
                        memory,
                        view,
                        width,
                        height,
                    });
                }
                Err(err) => {
                    unsafe {
                        device.free_memory(memory, None);
                        device.destroy_image(handle, None);
                    }
                    return Err(format!("Failed to create image view: {}", err));
                }
            };
        }

        Ok(image)
    }

    pub fn destroy(self: &Self, device: &ash::Device) {
        unsafe {
            if self.view != vk::ImageView::null() {
                device.destroy_image_view(self.view, None);
            }
            device.free_memory(self.memory, None);
            device.destroy_image(self.handle, None);
        }
    }

    fn create_view_for(
        self: &Self,
        device: &ash::Device,
        format: vk::Format,
        view_aspect_flags: vk::ImageAspectFlags,
    ) -> Result<vk::ImageView, String> {
        let view_create_info = vk::ImageViewCreateInfo {
            image: self.handle,
            view_type: vk::ImageViewType::TYPE_2D, //TODO: Support 3D images
            format,
            subresource_range: vk::ImageSubresourceRange {
                aspect_mask: view_aspect_flags,
                base_mip_level: 0, //TODO: Support mip levels
                level_count: 1,    //TODO: Support mip levels
                base_array_layer: 0,
                layer_count: 1, //TODO: Support array layers
            },
            ..Default::default()
        };

        match unsafe { device.create_image_view(&view_create_info, None) } {
            Ok(view) => Ok(view),
            Err(err) => Err(format!("Failed to create image view: {}", err)),
        }
    }
}
