use ash::vk::{self, Handle, Rect2D};

use super::{
    VulkanContext,
    commands::{VulkanCommandBuffer, VulkanCommandBufferState},
};

enum VulkanRenderPassState {
    Ready,
    Recording,
    InRenderPass,
    RencordingEnded,
    Submitted,
    NotAllocated,
}

pub(super) struct VulkanRenderPass {
    handle: vk::RenderPass,
    x: f32,
    y: f32,
    w: f32,
    h: f32,
    r: f32,
    g: f32,
    b: f32,
    a: f32,
    depth: f32,
    stencil: u32,

    state: VulkanRenderPassState,
}

impl VulkanRenderPass {
    // TODO: Replace with a proper Vector / Color definition ?
    pub fn new(
        context: &VulkanContext,
        x: f32,
        y: f32,
        w: f32,
        h: f32,
        r: f32,
        g: f32,
        b: f32,
        a: f32,
        depth: f32,
        stencil: u32,
    ) -> Result<Self, String> {
        let device = context.device.as_ref().unwrap().logical_device.as_ref().unwrap();

        // --- Output attachement (Color, Depth, Stencil) ---
        let attachment_descriptions = [
            // Color attachment
            vk::AttachmentDescription {
                format: context.swapchain.as_ref().unwrap().surface_format.format,
                samples: vk::SampleCountFlags::TYPE_1,
                load_op: vk::AttachmentLoadOp::CLEAR,
                store_op: vk::AttachmentStoreOp::STORE,
                stencil_load_op: vk::AttachmentLoadOp::DONT_CARE,
                stencil_store_op: vk::AttachmentStoreOp::DONT_CARE,
                initial_layout: vk::ImageLayout::UNDEFINED,
                final_layout: vk::ImageLayout::PRESENT_SRC_KHR,
                ..Default::default()
            },
            // Depth attachment
            vk::AttachmentDescription {
                format: context.device.as_ref().unwrap().depth_format,
                samples: vk::SampleCountFlags::TYPE_1,
                load_op: vk::AttachmentLoadOp::CLEAR,
                store_op: vk::AttachmentStoreOp::DONT_CARE,
                stencil_load_op: vk::AttachmentLoadOp::DONT_CARE,
                stencil_store_op: vk::AttachmentStoreOp::DONT_CARE,
                initial_layout: vk::ImageLayout::UNDEFINED,
                final_layout: vk::ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                ..Default::default()
            },
        ];
        let color_attachments_refs = [vk::AttachmentReference {
            attachment: 0,
            layout: vk::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
        }];
        let depth_attachment_ref = vk::AttachmentReference {
            attachment: 1,
            layout: vk::ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };

        let subpasses = [vk::SubpassDescription::default()
            .pipeline_bind_point(vk::PipelineBindPoint::GRAPHICS)
            .color_attachments(&color_attachments_refs)
            .depth_stencil_attachment(&depth_attachment_ref)];

        let dependencies = [vk::SubpassDependency {
            src_subpass: vk::SUBPASS_EXTERNAL,
            src_stage_mask: vk::PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT,
            dst_stage_mask: vk::PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT,
            dst_access_mask: vk::AccessFlags::COLOR_ATTACHMENT_WRITE
                | vk::AccessFlags::COLOR_ATTACHMENT_READ,
            ..Default::default()
        }];

        let render_pass_info = vk::RenderPassCreateInfo::default()
            .attachments(&attachment_descriptions)
            .dependencies(&dependencies)
            .subpasses(&subpasses);

        match unsafe { device.create_render_pass(&render_pass_info, None) } {
            Ok(handle) => Ok(VulkanRenderPass {
                handle,
                x,
                y,
                w,
                h,
                r,
                g,
                b,
                a,
                depth,
                stencil,
                state: VulkanRenderPassState::Ready,
            }),
            Err(err) => Err(format!("Failed to create render pass: {}", err)),
        }
    }

    pub fn destroy(self: &Self, context: &VulkanContext) {
        if !self.handle.is_null() {
            unsafe {
                let device = context.device.as_ref().unwrap().logical_device.as_ref().unwrap();
                device.destroy_render_pass(self.handle, None);
            }
        }
    }

    pub fn begin(
        self: &Self,
        context: &VulkanContext,
        cmds_buf: &mut VulkanCommandBuffer,
        framebuffer: vk::Framebuffer,
    ) {
        let device = context.device.as_ref().unwrap().logical_device.as_ref().unwrap();
        let clear_values = [
            vk::ClearValue {
                color: vk::ClearColorValue {
                    float32: [self.r, self.g, self.b, self.a],
                },
            },
            vk::ClearValue {
                depth_stencil: vk::ClearDepthStencilValue {
                    depth: self.depth,
                    stencil: self.stencil,
                },
            },
        ];
        let begin_info = vk::RenderPassBeginInfo::default()
            .render_pass(self.handle)
            .framebuffer(framebuffer)
            .render_area(Rect2D {
                offset: vk::Offset2D {
                    x: self.x as i32,
                    y: self.y as i32,
                },
                extent: vk::Extent2D {
                    width: self.w as u32,
                    height: self.h as u32,
                },
            })
            .clear_values(&clear_values);

        unsafe {
            device.cmd_begin_render_pass(
                cmds_buf.handle,
                &begin_info,
                vk::SubpassContents::INLINE,
            );
        }
        cmds_buf.state = VulkanCommandBufferState::InRenderPass;
    }

    pub fn end(self: &Self, context: &VulkanContext, cmds_buf: &mut VulkanCommandBuffer) {
        let device = context.device.as_ref().unwrap().logical_device.as_ref().unwrap();
        unsafe {
            device.cmd_end_render_pass(cmds_buf.handle);
        }
        cmds_buf.state = VulkanCommandBufferState::Recording;
    }
}
