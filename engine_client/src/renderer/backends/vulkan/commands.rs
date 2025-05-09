use ash::vk::{self, CommandBufferUsageFlags};

use crate::{command_buffer, command_buffer_mut, device, fence, swapchain, vulkan_device};

use super::VulkanRendererBackend;

pub(super) enum VulkanCommandBufferState {
    Ready,
    Recording,
    InRenderPass,
    RecordingEnded,
    Submitted,
    NotAllocated,
}

pub(super) struct VulkanCommandBuffer {
    pub handle: vk::CommandBuffer,
    pub state: VulkanCommandBufferState,
}

pub(super) struct VulkanCommandBufferOperations<'backend> {
    backend: &'backend mut VulkanRendererBackend,
}

pub(super) trait VulkanCommandBufferContext {
    fn commands(&mut self) -> VulkanCommandBufferOperations<'_>;
}

impl VulkanCommandBufferContext for VulkanRendererBackend {
    fn commands(&mut self) -> VulkanCommandBufferOperations<'_> {
        VulkanCommandBufferOperations { backend: self }
    }
}

impl VulkanCommandBuffer {
    pub fn new(
        device: &ash::Device,
        command_pool: vk::CommandPool,
        is_primary: bool,
    ) -> Result<Self, String> {
        let allocate_info = vk::CommandBufferAllocateInfo {
            command_pool,
            level: if is_primary {
                vk::CommandBufferLevel::PRIMARY
            } else {
                vk::CommandBufferLevel::SECONDARY
            },
            command_buffer_count: 1,
            ..Default::default()
        };

        let handle = match unsafe { device.allocate_command_buffers(&allocate_info) } {
            Ok(buffers) => buffers[0],
            Err(err) => return Err(format!("Failed to allocate command buffer: {}", err)),
        };

        Ok(Self {
            handle,
            state: VulkanCommandBufferState::Ready,
        })
    }

    pub fn destroy(self: &mut Self, device: &ash::Device, command_pool: vk::CommandPool) {
        unsafe {
            device.free_command_buffers(command_pool, &[self.handle]);
        }

        self.handle = vk::CommandBuffer::null();
        self.state = VulkanCommandBufferState::NotAllocated;
    }

    pub fn begin(
        self: &mut Self,
        device: &ash::Device,
        is_single_use: bool,
        is_renderpass_continue: bool,
        is_simultaneous_use: bool,
    ) -> Result<(), String> {
        let begin_info = vk::CommandBufferBeginInfo {
            flags: {
                let mut flags = CommandBufferUsageFlags::empty();
                if is_single_use {
                    flags |= vk::CommandBufferUsageFlags::ONE_TIME_SUBMIT
                }
                if is_renderpass_continue {
                    flags |= vk::CommandBufferUsageFlags::RENDER_PASS_CONTINUE
                }
                if is_simultaneous_use {
                    flags |= vk::CommandBufferUsageFlags::SIMULTANEOUS_USE
                }
                flags
            },
            ..Default::default()
        };

        match unsafe { device.begin_command_buffer(self.handle, &begin_info) } {
            Ok(_) => {
                self.state = VulkanCommandBufferState::Recording;
                Ok(())
            }
            Err(err) => Err(format!("Failed to begin command buffer: {}", err)),
        }
    }

    pub fn end(self: &mut Self, device: &ash::Device) -> Result<(), String> {
        match unsafe { device.end_command_buffer(self.handle) } {
            Ok(_) => {
                self.state = VulkanCommandBufferState::RecordingEnded;
                Ok(())
            }
            Err(err) => Err(format!("Failed to end command buffer: {}", err)),
        }
    }

    pub fn update_submitted(self: &mut Self) {
        self.state = VulkanCommandBufferState::Submitted;
    }

    pub fn reset(self: &mut Self) {
        self.state = VulkanCommandBufferState::Ready;
    }

    pub fn new_and_begin(
        device: &ash::Device,
        command_pool: vk::CommandPool,
    ) -> Result<Self, String> {
        let mut command_buffer = VulkanCommandBuffer::new(device, command_pool, true)?;
        command_buffer.begin(device, false, false, false)?;
        Ok(command_buffer)
    }

    pub fn end_submit_and_release(
        self: &mut Self,
        device: &ash::Device,
        command_pool: vk::CommandPool,
        queue: vk::Queue,
        fence: vk::Fence,
    ) -> Result<(), String> {
        self.end(device)?;

        let buffers = [self.handle.clone()];
        let submit_info = [vk::SubmitInfo::default().command_buffers(&buffers)];
        match unsafe { device.queue_submit(queue, &submit_info, fence) } {
            Ok(_) => {}
            Err(err) => return Err(format!("Failed to submit command buffer: {}", err)),
        }
        match unsafe { device.queue_wait_idle(queue) } {
            Ok(_) => {}
            Err(err) => return Err(format!("Failed to wait for queue idle: {}", err)),
        }

        self.destroy(device, command_pool);

        Ok(())
    }

    // --- Command Buffer Commands ---
    pub fn set_viewports(
        self: &mut Self,
        device: &ash::Device,
        viewports: Vec<vk::Viewport>,
    ) -> Result<(), String> {
        unsafe {
            device.cmd_set_viewport(self.handle, 0, &viewports);
        }
        Ok(())
    }

    pub fn set_scissors(
        self: &mut Self,
        device: &ash::Device,
        scissors: Vec<vk::Rect2D>,
    ) -> Result<(), String> {
        unsafe {
            device.cmd_set_scissor(self.handle, 0, &scissors);
        }
        Ok(())
    }

    pub fn copy_buffer(
        self: &mut Self,
        device: &ash::Device,
        src_buffer: vk::Buffer,
        dst_buffer: vk::Buffer,
        src_offset: vk::DeviceSize,
        dst_offset: vk::DeviceSize,
        size: vk::DeviceSize,
    ) -> Result<(), String> {
        let copy_region = vk::BufferCopy {
            src_offset,
            dst_offset,
            size,
            ..Default::default()
        };

        unsafe {
            device.cmd_copy_buffer(self.handle, src_buffer, dst_buffer, &[copy_region]);
        }
        Ok(())
    }

    pub fn bind_pipeline(
        self: &mut Self,
        device: &ash::Device,
        pipeline: vk::Pipeline,
        pipeline_type: vk::PipelineBindPoint,
    ) -> Result<(), String> {
        unsafe {
            device.cmd_bind_pipeline(self.handle, pipeline_type, pipeline);
        }
        Ok(())
    }

    pub fn bind_descriptor_sets(
        self: &mut Self,
        device: &ash::Device,
        pipeline_layout: vk::PipelineLayout,
        descriptor_sets: &[vk::DescriptorSet],
    ) -> Result<(), String> {
        unsafe {
            device.cmd_bind_descriptor_sets(
                self.handle,
                vk::PipelineBindPoint::GRAPHICS,
                pipeline_layout,
                0,
                descriptor_sets,
                &[],
            );
        }
        Ok(())
    }

    pub fn push_constants(
        self: &mut Self,
        device: &ash::Device,
        pipeline_layout: vk::PipelineLayout,
        offset: u32,
        data: &[u8],
    ) -> Result<(), String> {
        unsafe {
            device.cmd_push_constants(
                self.handle,
                pipeline_layout,
                vk::ShaderStageFlags::VERTEX,
                offset,
                data,
            );
        }
        Ok(())
    }
}

impl<'backend> VulkanCommandBufferOperations<'backend> {
    pub fn create_commands_buffers(&mut self) -> Result<(), String> {
        // If the context already has command buffers, destroy them
        // Extract needed values first to avoid multiple borrows
        let logical_device = device!(self.backend);
        let command_pool = vulkan_device!(self.backend).graphics_command_pool;

        for buffer in self.backend.graphics_cmds_buffers.iter_mut() {
            buffer.destroy(logical_device, command_pool);
        }
        self.backend.graphics_cmds_buffers.clear();

        // Create new command buffers
        // Extract all needed values first
        let swapchain = swapchain!(self.backend);
        let device = vulkan_device!(self.backend);
        let logical_device = device!(self.backend);
        let command_pool = device.graphics_command_pool;
        let image_count = swapchain.images.len();

        // Now create each buffer with the extracted values
        for _ in 0..image_count {
            let buffer = VulkanCommandBuffer::new(logical_device, command_pool, true)?;
            self.backend.graphics_cmds_buffers.push(buffer);
        }

        Ok(())
    }

    pub fn destroy_commands_buffers(&mut self) -> Result<(), String> {
        // Destroy Vulkan command buffers here
        // Extract needed values first to avoid multiple borrows
        let logical_device = device!(self.backend);
        let command_pool = vulkan_device!(self.backend).graphics_command_pool;

        // Now destroy each buffer with the extracted values
        for buffer in self.backend.graphics_cmds_buffers.iter_mut() {
            buffer.destroy(logical_device, command_pool);
        }
        self.backend.graphics_cmds_buffers.clear();
        Ok(())
    }

    pub fn begin_gfx_commands_buffer(&mut self) -> Result<(), String> {
        let command_buffer = command_buffer_mut!(self.backend, self.backend.image_index);
        let device = device!(self.backend);

        command_buffer.reset();
        command_buffer.begin(device, false, false, false)
    }

    pub fn end_gfx_commands_buffer(&mut self) -> Result<(), String> {
        let cmdsbuf = command_buffer_mut!(self.backend, self.backend.image_index);
        let device = device!(self.backend);

        cmdsbuf.end(device)?;
        Ok(())
    }

    pub fn submit_gfx_command_buf(&mut self) -> Result<(), String> {
        let device = device!(self.backend);
        let cmdsbuf = command_buffer!(self.backend, self.backend.image_index);
        let queue = vulkan_device!(self.backend).graphics_queue;
        let fence = fence!(self.backend, self.backend.current_frame);

        let commands_buffers = [cmdsbuf.handle];
        let wait_semaphores =
            [self.backend.image_available_semaphores[self.backend.current_frame]];
        let signal_semaphores =
            [self.backend.queue_complete_semaphores[self.backend.current_frame]];
        let pipeline_stage_flags = [vk::PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT];

        let submit_info = vk::SubmitInfo::default()
            .command_buffers(&commands_buffers)
            .wait_semaphores(&wait_semaphores)
            .signal_semaphores(&signal_semaphores)
            .wait_dst_stage_mask(&pipeline_stage_flags);

        match unsafe { device.queue_submit(queue, &[submit_info], fence.handle()) } {
            Ok(_) => {}
            Err(err) => return Err(format!("Failed to submit command buffer: {:?}", err)),
        }

        let cmdsbuf = command_buffer_mut!(self.backend, self.backend.image_index);
        cmdsbuf.update_submitted();

        Ok(())
    }
}
