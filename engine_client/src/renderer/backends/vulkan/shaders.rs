use std::io::Read;

use ash::vk::{self, Handle, MemoryMapFlags, ShaderStageFlags};
use glam::Mat4;

use crate::{
    builtin_object_shader, builtin_object_shader_mut, command_buffer, command_buffer_mut,
    device, graphics_pipeline, renderer::GlobalUniformObject,
};

use super::{VulkanBuffer, VulkanRendererBackend, buffers::VulkanBufferContext};

//TODO: This could be made configurable ?
const SHADER_DIR: &str = "assets/shaders/compiled";
const SHADER_EXTENSION: &str = "spv";

pub(super) struct VulkanObjectShader {
    pub shader_modules: Vec<VulkanShaderModule>,

    pub global_descriptor_pool: vk::DescriptorPool,
    pub global_descriptor_set_layout: vk::DescriptorSetLayout,
    // One descriptor set per frame - max 3 for triple buffering
    pub global_descriptor_set: Vec<vk::DescriptorSet>,

    pub global_uo: Option<GlobalUniformObject>,
    pub global_ubo: Option<VulkanBuffer>,
    pub global_descriptor_updated: Vec<bool>,

    pub pipeline: vk::Pipeline,
}

pub(super) struct VulkanShaderModule {
    pub handle: vk::ShaderModule,
    pub entry_point: String,
    pub shader_type: vk::ShaderStageFlags,
}

pub(super) struct VulkanShaderModuleOperations<'backend> {
    backend: &'backend mut VulkanRendererBackend,
}

pub(super) trait VulkanShaderModuleContext {
    fn shader_module(&mut self) -> VulkanShaderModuleOperations<'_>;
}

impl VulkanShaderModuleContext for VulkanRendererBackend {
    fn shader_module(&mut self) -> VulkanShaderModuleOperations<'_> {
        VulkanShaderModuleOperations { backend: self }
    }
}

impl VulkanShaderModule {
    pub fn new(
        device: &ash::Device,
        shader_code: &[u32],
        entry_point: &str,
        shader_type: vk::ShaderStageFlags,
    ) -> Result<Self, String> {
        let create_info = vk::ShaderModuleCreateInfo::default().code(shader_code);

        let handle = unsafe {
            device
                .create_shader_module(&create_info, None)
                .map_err(|err| format!("Failed to create shader module: {}", err))?
        };

        Ok(VulkanShaderModule {
            handle,
            entry_point: entry_point.to_string(),
            shader_type,
        })
    }

    pub fn destroy(&self, device: &ash::Device) {
        if self.handle.is_null() {
            return;
        }

        unsafe {
            device.destroy_shader_module(self.handle, None);
        }
    }
}

impl<'backend> VulkanShaderModuleOperations<'backend> {
    pub fn create(self: &mut Self, shader_name: &str) -> Result<(), String> {
        let (shader_code, shader_type) = self.load(shader_name)?;

        Ok(())
    }

    pub fn load_builtin_shaders(self: &mut Self) -> Result<(), String> {
        // Load the built-in shaders
        let shaders = ["default.builtin.vert", "default.builtin.frag"];

        let mut shader_modules = vec![];
        for shader_name in shaders.iter() {
            let (shader_code, shader_type) = self.load(shader_name)?;

            // Create the shader module
            let shader_module = VulkanShaderModule::new(
                device!(self.backend),
                &shader_code,
                "main",
                shader_type,
            )?;

            shader_modules.push(shader_module);
        }

        // Global descriptors
        let global_ubo_layout_binding = [vk::DescriptorSetLayoutBinding {
            binding: 0,
            descriptor_type: vk::DescriptorType::UNIFORM_BUFFER,
            descriptor_count: 1,
            stage_flags: vk::ShaderStageFlags::VERTEX,
            ..Default::default()
        }];

        let global_layout_create_info =
            vk::DescriptorSetLayoutCreateInfo::default().bindings(&global_ubo_layout_binding);

        let global_descriptor_set_layout = match unsafe {
            device!(self.backend)
                .create_descriptor_set_layout(&global_layout_create_info, None)
        } {
            Ok(layout) => layout,
            Err(err) => {
                return Err(format!("Failed to create descriptor set layout: {}", err));
            }
        };

        let global_pool_size = [vk::DescriptorPoolSize {
            ty: vk::DescriptorType::UNIFORM_BUFFER,
            descriptor_count: self.backend.images_in_flight.len() as u32,
            ..Default::default()
        }];

        let global_pool_create_info = vk::DescriptorPoolCreateInfo::default()
            .pool_sizes(&global_pool_size)
            .max_sets(self.backend.images_in_flight.len() as u32);

        let global_descriptor_pool = match unsafe {
            device!(self.backend).create_descriptor_pool(&global_pool_create_info, None)
        } {
            Ok(pool) => pool,
            Err(err) => {
                return Err(format!("Failed to create descriptor pool: {}", err));
            }
        };

        // Allocate the descriptor sets
        let global_layouts =
            vec![global_descriptor_set_layout; self.backend.images_in_flight.len()];

        let alloc_info = vk::DescriptorSetAllocateInfo::default()
            .descriptor_pool(global_descriptor_pool)
            .set_layouts(&global_layouts);

        let global_descriptor_set =
            match unsafe { device!(self.backend).allocate_descriptor_sets(&alloc_info) } {
                Ok(sets) => sets,
                Err(err) => {
                    return Err(format!("Failed to allocate descriptor sets: {}", err));
                }
            };

        // Create the VulkanObjectShader
        self.backend.builtin_object_shader = Some(VulkanObjectShader {
            shader_modules,
            global_descriptor_pool,
            global_descriptor_set_layout,
            global_descriptor_set,
            global_uo: None,
            global_ubo: None,
            global_descriptor_updated: vec![false; self.backend.images_in_flight.len()],
            pipeline: vk::Pipeline::null(),
        });

        //NOTE: This need to be done after the descriptor set is created
        self.backend.buffer().create_uniform_buffer()?;

        Ok(())
    }

    pub fn update_global_state(&mut self) -> Result<(), String> {
        let image_index = self.backend.image_index;
        let cmds_buf = command_buffer_mut!(self.backend, image_index);
        let global_descriptor =
            [builtin_object_shader!(self.backend).global_descriptor_set[image_index]];

        // Configure the descriptors for the given index.
        // Update the global uniform buffer
        let builtin_object_shader = builtin_object_shader!(self.backend);
        if !builtin_object_shader.global_descriptor_updated[image_index] {
            if let Some(ubo) = &builtin_object_shader.global_ubo {
                // Copy the data to buffer
                if let Some(global_uo) = builtin_object_shader.global_uo {
                    let size = std::mem::size_of::<GlobalUniformObject>() as u64;
                    let offset = size * image_index as u64;
                    ubo.load_data(
                        device!(self.backend),
                        offset,
                        size,
                        MemoryMapFlags::empty(),
                        vec![global_uo],
                    )?;

                    let buffer_info = [vk::DescriptorBufferInfo {
                        buffer: ubo.handle,
                        offset,
                        range: size,
                    }];

                    let descriptor_write = vk::WriteDescriptorSet::default()
                        .dst_set(global_descriptor[0])
                        .dst_binding(0)
                        .descriptor_count(1)
                        .descriptor_type(vk::DescriptorType::UNIFORM_BUFFER)
                        .buffer_info(&buffer_info);

                    unsafe {
                        device!(self.backend).update_descriptor_sets(&[descriptor_write], &[]);
                    }
                }
            }
            builtin_object_shader_mut!(self.backend).global_descriptor_updated[image_index] =
                true;
        }

        // Bind the global descriptor set to be updated.
        // NOTE: Some new GPU drivers require to bind the descriptor set only after the update
        cmds_buf.bind_descriptor_sets(
            device!(self.backend),
            graphics_pipeline!(self.backend).layout,
            &global_descriptor,
        )?;

        Ok(())
    }

    pub fn update_object(&mut self, model: Mat4) -> Result<(), String> {
        let image_index = self.backend.image_index;
        let cmds_buf = command_buffer_mut!(self.backend, image_index);

        // Convert the model matrix to a byte array
        let model_bytes = unsafe {
            std::slice::from_raw_parts(
                &model as *const Mat4 as *const u8,
                std::mem::size_of::<Mat4>(),
            )
        };

        // Update the push constant with the model matrix
        cmds_buf.push_constants(
            device!(self.backend),
            graphics_pipeline!(self.backend).layout,
            0,
            model_bytes,
        )
    }

    pub fn destroy_builtin_shaders(self: &mut Self) -> Result<(), String> {
        // Destroy the built-in shaders
        if let Some(mut object_shader) = self.backend.builtin_object_shader.take() {
            // Destroy the uniform buffer
            if let Some(ubo) = object_shader.global_ubo.take() {
                ubo.destroy(device!(self.backend));
            }

            // Destroy the global descriptor pool
            unsafe {
                device!(self.backend)
                    .destroy_descriptor_pool(object_shader.global_descriptor_pool, None);
            }

            // Destroy the global descriptor set layout
            unsafe {
                device!(self.backend).destroy_descriptor_set_layout(
                    object_shader.global_descriptor_set_layout,
                    None,
                );
            }

            for shader_module in object_shader.shader_modules.iter() {
                shader_module.destroy(device!(self.backend));
            }
        }
        self.backend.builtin_object_shader = None;

        Ok(())
    }

    //NOTE: For now, we just load the shader code directly from the file
    //      In the future, we might want to create some kinf of pakage system
    //      to load the shaders (and assets) from a compressed file
    fn load(
        self: &mut Self,
        shader_name: &str,
    ) -> Result<(Vec<u32>, ShaderStageFlags), String> {
        // Load the shader code from the file
        let shader_path = format!("{}/{}.{}", SHADER_DIR, shader_name, SHADER_EXTENSION);

        log::debug!("Loading shader: {}", shader_path);

        //NOTE: By convention, the shader name should be suffixed with the shader type
        //       (e.g. "my_shader.vert", "my_shader.frag", etc.)
        let stage = shader_name.rsplitn(3, '.').nth(0);
        let shader_type = match stage {
            Some("vert") => vk::ShaderStageFlags::VERTEX,
            Some("frag") => vk::ShaderStageFlags::FRAGMENT,
            Some("comp") => vk::ShaderStageFlags::COMPUTE,
            Some("geom") => vk::ShaderStageFlags::GEOMETRY,
            Some("tesc") => vk::ShaderStageFlags::TESSELLATION_CONTROL,
            Some("tese") => vk::ShaderStageFlags::TESSELLATION_EVALUATION,
            _ => return Err(format!("Unknown shader type: {}", shader_name)),
        };

        // Try to open the shader file
        let mut file = std::fs::File::open(&shader_path)
            .map_err(|_| format!("Failed to open shader file: {}", shader_path))?;
        let mut buffer = Vec::new();
        file.read_to_end(&mut buffer)
            .map_err(|_| format!("Failed to read shader file: {}", shader_path))?;

        // Convert the byte buffer to u32
        let code = unsafe {
            std::slice::from_raw_parts(buffer.as_ptr() as *const u32, buffer.len() / 4)
        };

        Ok((code.to_vec(), shader_type))
    }
}
