use ash::vk::{self, CommandBufferUsageFlags};

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
    ) -> Result<(), String> {
        self.end(device)?;

        let buffers = [self.handle.clone()];
        let submit_info = [vk::SubmitInfo::default().command_buffers(&buffers)];
        match unsafe { device.queue_submit(queue, &submit_info, vk::Fence::null()) } {
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
}
