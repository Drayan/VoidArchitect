use ash::vk::Handle;

use super::renderpass::VulkanRenderPass;

pub(super) struct VulkanFramebuffer {
    pub handle: ash::vk::Framebuffer,
    pub attachments: Vec<ash::vk::ImageView>,
    pub renderpass: VulkanRenderPass,
}

impl VulkanFramebuffer {
    pub fn new(
        device: &ash::Device,
        width: u32,
        height: u32,
        renderpass: VulkanRenderPass,
        attachments: Vec<ash::vk::ImageView>,
    ) -> Result<Self, String> {
        // Create framebuffer
        let framebuffer_info = ash::vk::FramebufferCreateInfo::default()
            .attachments(&attachments)
            .width(width)
            .height(height)
            .layers(1)
            .render_pass(renderpass.handle);

        let handle = match unsafe { device.create_framebuffer(&framebuffer_info, None) } {
            Ok(handle) => handle,
            Err(err) => {
                return Err(format!("Failed to create framebuffer: {}", err));
            }
        };

        Ok(VulkanFramebuffer {
            handle,
            attachments,
            renderpass,
        })
    }

    pub fn destroy(&self, device: &ash::Device) {
        if !self.handle.is_null() {
            unsafe {
                device.destroy_framebuffer(self.handle, None);
            }
        }
    }
}
