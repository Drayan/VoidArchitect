use ash::vk::Handle;

use crate::{device, main_renderpass, swapchain_mut};

use super::{VulkanRendererBackend, renderpass::VulkanRenderPass};

pub(super) struct VulkanFramebuffer {
    pub handle: ash::vk::Framebuffer,
    pub attachments: Vec<ash::vk::ImageView>,
    pub renderpass: VulkanRenderPass,
}

pub(super) struct VulkanFramebufferOperations<'backend> {
    backend: &'backend mut VulkanRendererBackend,
}

pub(super) trait VulkanFramebufferContext {
    fn framebuffer(&mut self) -> VulkanFramebufferOperations<'_>;
}

impl VulkanFramebufferContext for VulkanRendererBackend {
    fn framebuffer(&mut self) -> VulkanFramebufferOperations<'_> {
        VulkanFramebufferOperations { backend: self }
    }
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

impl<'backend> VulkanFramebufferOperations<'backend> {
    pub fn recreate_framebuffers(&mut self) -> Result<(), String> {
        let swapchain = swapchain_mut!(self.backend);
        let device = device!(self.backend);
        let renderpass = main_renderpass!(self.backend);
        let width = self.backend.framebuffer_width;
        let height = self.backend.framebuffer_height;

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
