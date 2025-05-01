use ash::vk::{self, Handle, Rect2D};

use crate::{command_buffer_mut, device, framebuffer, main_renderpass, main_renderpass_mut};

use super::{
    VulkanRendererBackend,
    commands::{VulkanCommandBuffer, VulkanCommandBufferState},
};

#[derive(Clone)]
enum VulkanRenderPassState {
    Ready,
    Recording,
    InRenderPass,
    RencordingEnded,
    Submitted,
    NotAllocated,
}

#[derive(Clone)]
pub(super) struct VulkanRenderPass {
    pub handle: vk::RenderPass,
    pub x: f32,
    pub y: f32,
    pub w: f32,
    pub h: f32,
    pub r: f32,
    pub g: f32,
    pub b: f32,
    pub a: f32,
    depth: f32,
    stencil: u32,

    state: VulkanRenderPassState,
}

pub(super) struct VulkanRenderPassOperations<'backend> {
    backend: &'backend mut VulkanRendererBackend,
}

pub(super) trait VulkanRenderPassContext {
    fn renderpass(&mut self) -> VulkanRenderPassOperations<'_>;
}

impl VulkanRenderPassContext for VulkanRendererBackend {
    fn renderpass(&mut self) -> VulkanRenderPassOperations<'_> {
        VulkanRenderPassOperations { backend: self }
    }
}

impl VulkanRenderPass {
    // TODO: Replace with a proper Vector / Color definition ?
    pub fn new(
        device: &ash::Device,
        color_format: vk::Format,
        depth_format: vk::Format,
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
        // --- Output attachement (Color, Depth, Stencil) ---
        let attachment_descriptions = [
            // Color attachment
            vk::AttachmentDescription {
                format: color_format,
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
                format: depth_format,
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

    pub fn destroy(self: &Self, device: &ash::Device) {
        if !self.handle.is_null() {
            unsafe {
                device.destroy_render_pass(self.handle, None);
            }
        }
    }

    pub fn begin(
        self: &Self,
        device: &ash::Device,
        cmds_buf: &mut VulkanCommandBuffer,
        framebuffer: vk::Framebuffer,
    ) {
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

    pub fn end(self: &Self, device: &ash::Device, cmds_buf: &mut VulkanCommandBuffer) {
        unsafe {
            device.cmd_end_render_pass(cmds_buf.handle);
        }
        cmds_buf.state = VulkanCommandBufferState::Recording;
    }
}

impl<'backend> VulkanRenderPassOperations<'backend> {
    pub fn begin_main_render_pass(&mut self) -> Result<(), String> {
        let width = self.backend.framebuffer_width;
        let height = self.backend.framebuffer_height;
        let framebuffer = framebuffer!(self.backend, self.backend.image_index);
        let device = device!(self.backend);
        let command_buffer = command_buffer_mut!(self.backend, self.backend.image_index);

        // Set viewport and scissor
        let viewport = vk::Viewport {
            x: 0.0,
            y: height as f32,
            width: width as f32,
            height: -(height as f32),
            min_depth: 0.0,
            max_depth: 1.0,
        };

        let scissor = vk::Rect2D {
            offset: vk::Offset2D { x: 0, y: 0 },
            extent: vk::Extent2D { width, height },
        };

        let renderpass = main_renderpass_mut!(self.backend);
        renderpass.w = self.backend.framebuffer_width as f32;
        renderpass.h = self.backend.framebuffer_height as f32;

        command_buffer.set_viewports(device, [viewport].to_vec())?;
        command_buffer.set_scissors(device, [scissor].to_vec())?;

        renderpass.begin(device, command_buffer, framebuffer.handle);

        Ok(())
    }

    pub fn end_main_render_pass(&mut self) -> Result<(), String> {
        let cmdsbuf = command_buffer_mut!(self.backend, self.backend.image_index);
        let device = device!(self.backend);
        let renderpass = main_renderpass!(self.backend);

        renderpass.end(device, cmdsbuf);
        Ok(())
    }
}
