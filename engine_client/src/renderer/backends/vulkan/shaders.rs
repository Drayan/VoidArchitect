use std::io::Read;

use ash::vk::{self, Handle, ShaderStageFlags};

use crate::device;

use super::VulkanRendererBackend;

//TODO: This could be made configurable ?
const SHADER_DIR: &str = "assets/shaders/compiled";
const SHADER_EXTENSION: &str = "spv";

pub(super) struct VulkanShaderModule {
    pub handle: ash::vk::ShaderModule,
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

    pub fn bind(
        &self,
        device: &ash::Device,
        command_buffer: vk::CommandBuffer,
    ) -> Result<(), String> {
        // Bind the shader module to the command buffer
        let bind_info = vk::PipelineShaderStageCreateInfo::default()
            .stage(self.shader_type)
            .module(self.handle)
            .name(c"main");

        Ok(())
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

        for shader_name in shaders.iter() {
            let (shader_code, shader_type) = self.load(shader_name)?;

            // Create the shader module
            let shader_module = VulkanShaderModule::new(
                device!(self.backend),
                &shader_code,
                "main",
                shader_type,
            )?;

            // Store the shader module in the backend
            self.backend.shader_modules.push(shader_module);
        }

        //TODO: Descriptors ?

        Ok(())
    }

    pub fn destroy_builtin_shaders(self: &mut Self) -> Result<(), String> {
        // Destroy the built-in shaders
        for shader_module in self.backend.shader_modules.iter() {
            shader_module.destroy(device!(self.backend));
        }

        self.backend.shader_modules.clear();
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
