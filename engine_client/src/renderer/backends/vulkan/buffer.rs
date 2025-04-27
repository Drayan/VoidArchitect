//! Vulkan buffer management module.
//!
//! This module provides structures and functions for creating and managing Vulkan buffers,
//! including vertex buffers and memory allocation.

use ash::vk;
use std::mem::size_of;

/// Represents a vertex with position and color attributes.
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct Vertex {
    /// 3D position of the vertex
    pub pos: [f32; 3],
    /// RGB color of the vertex
    pub color: [f32; 3],
}

impl Vertex {
    /// Creates a new vertex with the given position and color.
    pub fn new(pos: [f32; 3], color: [f32; 3]) -> Self {
        Self { pos, color }
    }

    /// Returns the binding description for this vertex type.
    pub fn get_binding_description() -> vk::VertexInputBindingDescription {
        vk::VertexInputBindingDescription {
            binding: 0,
            stride: size_of::<Vertex>() as u32,
            input_rate: vk::VertexInputRate::VERTEX,
        }
    }

    /// Returns the attribute descriptions for this vertex type.
    pub fn get_attribute_descriptions() -> [vk::VertexInputAttributeDescription; 2] {
        [
            // Position attribute
            vk::VertexInputAttributeDescription {
                binding: 0,
                location: 0,
                format: vk::Format::R32G32B32_SFLOAT,
                offset: 0,
            },
            // Color attribute
            vk::VertexInputAttributeDescription {
                binding: 0,
                location: 1,
                format: vk::Format::R32G32B32_SFLOAT,
                offset: size_of::<[f32; 3]>() as u32,
            },
        ]
    }
}

/// Enum representing the type of buffer.
#[derive(Debug, Clone, Copy)]
pub enum BufferType {
    /// Vertex buffer
    Vertex,
    /// Index buffer
    Index,
}

impl BufferType {
    /// Returns the Vulkan buffer usage flags for this buffer type.
    pub fn usage_flags(&self) -> vk::BufferUsageFlags {
        match self {
            BufferType::Vertex => vk::BufferUsageFlags::VERTEX_BUFFER,
            BufferType::Index => vk::BufferUsageFlags::INDEX_BUFFER,
        }
    }
}

/// Represents a Vulkan buffer and its associated memory.
pub struct VulkanBuffer {
    /// The Vulkan buffer handle
    pub buffer: vk::Buffer,
    /// The Vulkan memory handle
    pub memory: vk::DeviceMemory,
    /// The size of the buffer in bytes
    pub size: vk::DeviceSize,
    /// The type of buffer
    pub buffer_type: BufferType,
}

impl VulkanBuffer {
    /// Creates a new Vulkan buffer with the specified size, type, and memory properties.
    ///
    /// # Arguments
    /// * `device` - The logical device.
    /// * `size` - The size of the buffer in bytes.
    /// * `buffer_type` - The type of buffer (vertex or index).
    /// * `properties` - The memory property flags.
    /// * `find_memory_type_fn` - Function to find a suitable memory type.
    ///
    /// # Returns
    /// * `Result<VulkanBuffer, String>` - The created buffer or an error message.
    pub fn new(
        device: &ash::Device,
        size: vk::DeviceSize,
        buffer_type: BufferType,
        properties: vk::MemoryPropertyFlags,
        find_memory_type_fn: impl Fn(u32, vk::MemoryPropertyFlags) -> Result<u32, String>,
    ) -> Result<Self, String> {
        // Create buffer
        let buffer_info = vk::BufferCreateInfo::default()
            .size(size)
            .usage(buffer_type.usage_flags())
            .sharing_mode(vk::SharingMode::EXCLUSIVE);

        let buffer = unsafe {
            device
                .create_buffer(&buffer_info, None)
                .map_err(|e| format!("Failed to create buffer: {}", e))?
        };

        // Get memory requirements
        let mem_requirements = unsafe { device.get_buffer_memory_requirements(buffer) };

        // Allocate memory
        let alloc_info = vk::MemoryAllocateInfo::default()
            .allocation_size(mem_requirements.size)
            .memory_type_index(find_memory_type_fn(
                mem_requirements.memory_type_bits,
                properties,
            )?);

        let memory = unsafe {
            device
                .allocate_memory(&alloc_info, None)
                .map_err(|e| format!("Failed to allocate buffer memory: {}", e))?
        };

        // Bind memory to buffer
        unsafe {
            device
                .bind_buffer_memory(buffer, memory, 0)
                .map_err(|e| format!("Failed to bind buffer memory: {}", e))?;
        }

        Ok(Self {
            buffer,
            memory,
            size,
            buffer_type,
        })
    }

    /// Creates a vertex buffer from an array of vertices.
    ///
    /// # Arguments
    /// * `device` - The logical device.
    /// * `vertices` - The array of vertices.
    /// * `find_memory_type_fn` - Function to find a suitable memory type.
    ///
    /// # Returns
    /// * `Result<VulkanBuffer, String>` - The created vertex buffer or an error message.
    pub fn create_vertex_buffer(
        device: &ash::Device,
        vertices: &[Vertex],
        find_memory_type_fn: impl Fn(u32, vk::MemoryPropertyFlags) -> Result<u32, String>,
    ) -> Result<Self, String> {
        let buffer_size = (size_of::<Vertex>() * vertices.len()) as vk::DeviceSize;

        // Create a host-visible buffer for easy data upload
        let buffer = Self::new(
            device,
            buffer_size,
            BufferType::Vertex,
            vk::MemoryPropertyFlags::HOST_VISIBLE | vk::MemoryPropertyFlags::HOST_COHERENT,
            find_memory_type_fn,
        )?;

        // Map memory and copy data
        buffer.upload_data(device, vertices)?;

        Ok(buffer)
    }

    /// Uploads data to the buffer.
    ///
    /// # Arguments
    /// * `device` - The logical device.
    /// * `data` - The data to upload.
    ///
    /// # Returns
    /// * `Result<(), String>` - Success or an error message.
    pub fn upload_data<T: Copy>(
        &self,
        device: &ash::Device,
        data: &[T],
    ) -> Result<(), String> {
        let data_size = (size_of::<T>() * data.len()) as vk::DeviceSize;
        if data_size > self.size {
            return Err(format!(
                "Data size ({} bytes) exceeds buffer size ({} bytes)",
                data_size, self.size
            ));
        }

        // Map memory
        let memory_ptr = unsafe {
            device
                .map_memory(self.memory, 0, self.size, vk::MemoryMapFlags::empty())
                .map_err(|e| format!("Failed to map memory: {}", e))?
        };

        // Copy data
        unsafe {
            std::ptr::copy_nonoverlapping(
                data.as_ptr() as *const std::ffi::c_void,
                memory_ptr,
                data_size as usize,
            );
        }

        // Unmap memory
        unsafe {
            device.unmap_memory(self.memory);
        }

        Ok(())
    }

    /// Destroys the buffer and frees its memory.
    ///
    /// # Arguments
    /// * `device` - The logical device.
    pub fn destroy(&self, device: &ash::Device) {
        unsafe {
            device.destroy_buffer(self.buffer, None);
            device.free_memory(self.memory, None);
        }
    }
}
