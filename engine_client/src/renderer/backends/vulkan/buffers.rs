use std::ffi;

use ash::vk;
use glam::Vec3;
use sdl3::sys::vulkan;

use crate::{device, instance, renderer::Vertex, vulkan_device};

use super::{VulkanCommandBuffer, VulkanFence, VulkanRendererBackend};

pub(super) struct VulkanBuffer {
    pub handle: vk::Buffer,
    pub memory: vk::DeviceMemory,
    pub size: vk::DeviceSize,
    pub usage: vk::BufferUsageFlags,

    pub is_locked: bool,

    pub memory_index: u32,
    pub memory_type: vk::MemoryPropertyFlags,
}

pub(super) struct VulkanBufferOperations<'backend> {
    backend: &'backend mut VulkanRendererBackend,
}

pub(super) trait VulkanBufferContext {
    fn buffer(&mut self) -> VulkanBufferOperations<'_>;
}

impl VulkanBufferContext for VulkanRendererBackend {
    fn buffer(&mut self) -> VulkanBufferOperations<'_> {
        VulkanBufferOperations { backend: self }
    }
}

impl VulkanBuffer {
    pub fn new(
        physical_device: ash::vk::PhysicalDevice,
        instance: &ash::Instance,
        device: &ash::Device,
        size: vk::DeviceSize,
        usage: vk::BufferUsageFlags,
        memory_type: vk::MemoryPropertyFlags,
        bind_on_create: bool,
    ) -> Result<Self, String> {
        let buffer_info = vk::BufferCreateInfo {
            size,
            usage,
            sharing_mode: vk::SharingMode::EXCLUSIVE,
            ..Default::default()
        };

        let handle = unsafe {
            device
                .create_buffer(&buffer_info, None)
                .map_err(|err| format!("Failed to create buffer: {}", err))?
        };

        let memory_requirements = unsafe { device.get_buffer_memory_requirements(handle) };
        let memory_index = VulkanRendererBackend::find_memory_type(
            physical_device,
            instance,
            memory_requirements.memory_type_bits,
            memory_type,
        )?;

        let memory_info = vk::MemoryAllocateInfo {
            allocation_size: memory_requirements.size,
            memory_type_index: memory_index,
            ..Default::default()
        };

        let memory = unsafe {
            device
                .allocate_memory(&memory_info, None)
                .map_err(|err| format!("Failed to allocate buffer memory: {}", err))?
        };

        if bind_on_create {
            unsafe {
                device
                    .bind_buffer_memory(handle, memory, 0)
                    .map_err(|err| format!("Failed to bind buffer memory: {}", err))?;
            }
        }

        Ok(VulkanBuffer {
            handle,
            memory,
            size,
            usage,
            is_locked: false,
            memory_index,
            memory_type,
        })
    }

    pub fn destroy(&self, device: &ash::Device) {
        if self.handle == vk::Buffer::null() {
            return;
        }

        unsafe {
            device.free_memory(self.memory, None);
            device.destroy_buffer(self.handle, None);
        }
    }

    pub fn resize(
        &mut self,
        physical_device: ash::vk::PhysicalDevice,
        instance: &ash::Instance,
        device: &ash::Device,
        new_size: vk::DeviceSize,
        queue: vk::Queue,
        command_pool: vk::CommandPool,
    ) -> Result<(), String> {
        if new_size == self.size {
            return Ok(());
        }
        if new_size < self.size {
            return Err("New size is smaller than current size".to_string());
        }

        let mut new_buffer = VulkanBuffer::new(
            physical_device,
            instance,
            device,
            new_size,
            self.usage,
            self.memory_type,
            true,
        )?;

        let mut fence = VulkanFence::new(device, false)?;

        self.copy_to_buffer(
            device,
            command_pool,
            Some(fence.handle()),
            queue,
            0,
            &mut new_buffer,
            0,
        )?;

        fence.wait(device, u64::MAX)?;
        fence.destroy(device);

        self.destroy(device);

        self.handle = new_buffer.handle;
        self.memory = new_buffer.memory;
        self.size = new_buffer.size;
        self.usage = new_buffer.usage;
        self.is_locked = new_buffer.is_locked;
        self.memory_index = new_buffer.memory_index;
        self.memory_type = new_buffer.memory_type;

        Ok(())
    }

    pub fn bind(&self, device: &ash::Device, offset: u64) -> Result<(), String> {
        unsafe {
            device
                .bind_buffer_memory(self.handle, self.memory, offset)
                .map_err(|err| format!("Failed to bind buffer memory: {}", err))?
        };
        Ok(())
    }

    pub fn lock_memory(
        &self,
        device: &ash::Device,
        offset: vk::DeviceSize,
        size: vk::DeviceSize,
        flags: vk::MemoryMapFlags,
    ) -> Result<*mut ffi::c_void, String> {
        let data = unsafe {
            device
                .map_memory(self.memory, offset, size, flags)
                .map_err(|err| format!("Failed to lock buffer memory: {}", err))?
        };
        Ok(data)
    }

    pub fn unlock_memory(&self, device: &ash::Device) -> Result<(), String> {
        unsafe { device.unmap_memory(self.memory) };
        Ok(())
    }

    pub fn load_data<T>(
        &self,
        device: &ash::Device,
        offset: vk::DeviceSize,
        size: vk::DeviceSize,
        flags: vk::MemoryMapFlags,
        data: Vec<T>,
    ) -> Result<(), String> {
        let data_ptr = match self.lock_memory(device, offset, size, flags) {
            Ok(ptr) => ptr,
            Err(err) => return Err(format!("Failed to lock buffer memory: {}", err)),
        };
        unsafe {
            std::ptr::copy_nonoverlapping(
                data.as_ptr() as *const ffi::c_void,
                data_ptr,
                data.len() * std::mem::size_of::<T>(),
            );
        }
        self.unlock_memory(device)?;
        Ok(())
    }

    pub fn copy_to_buffer(
        &self,
        device: &ash::Device,
        command_pool: vk::CommandPool,
        fence: Option<vk::Fence>,
        queue: vk::Queue,
        src_offset: u64,
        dest_buffer: &mut VulkanBuffer,
        dest_offset: u64,
    ) -> Result<(), String> {
        unsafe {
            device
                .queue_wait_idle(queue)
                .map_err(|err| format!("Failed to wait for queue: {}", err))?;
        }

        // Prepare the copy command and add it to the command buffer
        let mut cmds_buf = VulkanCommandBuffer::new_and_begin(device, command_pool)?;
        cmds_buf.copy_buffer(
            device,
            self.handle,
            dest_buffer.handle,
            src_offset,
            dest_offset,
            self.size,
        )?;
        cmds_buf.end_submit_and_release(
            device,
            command_pool,
            queue,
            if fence.is_some() {
                *fence.as_ref().ok_or("Failed to take fence")?
            } else {
                vk::Fence::null()
            },
        )?;

        Ok(())
    }
}

impl<'backend> VulkanBufferOperations<'backend> {
    //TODO: In the future, this could be splitted into two different operations
    // one for creating the buffer and one for copying the data
    pub fn create_vertices_buffer(&mut self, vertices: Vec<Vec3>) -> Result<(), String> {
        let size =
            std::mem::size_of::<Vertex>() as vk::DeviceSize * vertices.len() as vk::DeviceSize;
        let device = device!(self.backend);
        let mut buffer = VulkanBuffer::new(
            vulkan_device!(self.backend).physical_device,
            instance!(self.backend),
            device,
            size,
            vk::BufferUsageFlags::VERTEX_BUFFER
                | vk::BufferUsageFlags::TRANSFER_DST
                | vk::BufferUsageFlags::TRANSFER_SRC,
            vk::MemoryPropertyFlags::DEVICE_LOCAL,
            true,
        )?;

        let staging = VulkanBuffer::new(
            vulkan_device!(self.backend).physical_device,
            instance!(self.backend),
            device,
            size,
            vk::BufferUsageFlags::TRANSFER_SRC,
            vk::MemoryPropertyFlags::HOST_VISIBLE | vk::MemoryPropertyFlags::HOST_COHERENT,
            true,
        )?;

        staging.load_data(device, 0, size, vk::MemoryMapFlags::empty(), vertices)?;

        let mut fence = VulkanFence::new(device!(self.backend), false)?;
        staging.copy_to_buffer(
            device,
            vulkan_device!(self.backend).graphics_command_pool,
            Some(fence.handle()),
            vulkan_device!(self.backend).graphics_queue,
            0,
            &mut buffer,
            0,
        )?;
        fence.wait(device, u64::MAX)?;
        fence.destroy(device);
        staging.destroy(device);

        //TODO: This should probably be stored somewhere else
        self.backend.object_vertex_buffer = Some(buffer);
        Ok(())
    }

    pub fn create_indices_buffer(&mut self, indices: Vec<u32>) -> Result<(), String> {
        let size =
            std::mem::size_of::<u32>() as vk::DeviceSize * indices.len() as vk::DeviceSize;
        let device = device!(self.backend);
        let mut buffer = VulkanBuffer::new(
            vulkan_device!(self.backend).physical_device,
            instance!(self.backend),
            device,
            size,
            vk::BufferUsageFlags::INDEX_BUFFER
                | vk::BufferUsageFlags::TRANSFER_DST
                | vk::BufferUsageFlags::TRANSFER_SRC,
            vk::MemoryPropertyFlags::DEVICE_LOCAL,
            true,
        )?;

        let staging = VulkanBuffer::new(
            vulkan_device!(self.backend).physical_device,
            instance!(self.backend),
            device,
            size,
            vk::BufferUsageFlags::TRANSFER_SRC,
            vk::MemoryPropertyFlags::HOST_VISIBLE | vk::MemoryPropertyFlags::HOST_COHERENT,
            true,
        )?;

        staging.load_data(device, 0, size, vk::MemoryMapFlags::empty(), indices)?;

        let mut fence = VulkanFence::new(device!(self.backend), false)?;
        staging.copy_to_buffer(
            device,
            vulkan_device!(self.backend).graphics_command_pool,
            Some(fence.handle()),
            vulkan_device!(self.backend).graphics_queue,
            0,
            &mut buffer,
            0,
        )?;
        fence.wait(device, u64::MAX)?;
        fence.destroy(device);
        staging.destroy(device);

        //TODO: This should probably be stored somewhere else
        self.backend.object_index_buffer = Some(buffer);
        Ok(())
    }

    pub fn create_uniform_buffer(&self) -> Result<(), String> {
        Ok(())
    }

    pub fn destroy_buffers(&mut self) -> Result<(), String> {
        // Destroy Vulkan buffers here
        // Extract needed values first to avoid multiple borrows
        let device = device!(self.backend);

        // Now destroy each buffer with the extracted values
        if let Some(buffer) = self.backend.object_vertex_buffer.take() {
            buffer.destroy(device);
        }
        if let Some(buffer) = self.backend.object_index_buffer.take() {
            buffer.destroy(device);
        }

        Ok(())
    }
}
