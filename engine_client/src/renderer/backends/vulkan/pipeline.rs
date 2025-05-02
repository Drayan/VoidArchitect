use ash::vk;

use crate::{device, main_renderpass, renderer::Vertex};

use super::{VulkanRenderPass, VulkanRendererBackend};

pub(super) enum PipelineType {
    Graphics,
    Compute,
}

pub(super) struct VulkanPipeline {
    pub handle: ash::vk::Pipeline,
    pub layout: ash::vk::PipelineLayout,
    pub render_pass: ash::vk::RenderPass,
    pub pipeline_type: PipelineType,
}

pub(super) struct VulkanPipelineOperations<'backend> {
    pub backend: &'backend mut VulkanRendererBackend,
}

pub(super) trait VulkanPipelineContext {
    fn pipeline(&mut self) -> VulkanPipelineOperations<'_>;
}

impl VulkanPipelineContext for VulkanRendererBackend {
    fn pipeline(&mut self) -> VulkanPipelineOperations<'_> {
        VulkanPipelineOperations { backend: self }
    }
}

impl VulkanPipeline {
    pub fn new(
        device: &ash::Device,
        pipeline_type: PipelineType,
        viewports: Vec<vk::Viewport>,
        scissors: Vec<vk::Rect2D>,
        attribute_descriptions: Vec<vk::VertexInputAttributeDescription>,
        descriptor_set_layouts: Vec<vk::DescriptorSetLayout>,
        shader_stages: Vec<vk::PipelineShaderStageCreateInfo>,
        renderpass: &VulkanRenderPass,
        is_wireframe: bool,
    ) -> Result<Self, String> {
        let viewport_state = vk::PipelineViewportStateCreateInfo::default()
            .viewports(&viewports)
            .scissors(&scissors);

        let rasterization_state = vk::PipelineRasterizationStateCreateInfo::default()
            .polygon_mode(if is_wireframe {
                vk::PolygonMode::LINE
            } else {
                vk::PolygonMode::FILL
            })
            .cull_mode(vk::CullModeFlags::BACK)
            .front_face(vk::FrontFace::COUNTER_CLOCKWISE)
            .line_width(1.0);

        let multisample_state = vk::PipelineMultisampleStateCreateInfo::default()
            .rasterization_samples(vk::SampleCountFlags::TYPE_1)
            .sample_shading_enable(false);

        let color_blend_attachment_state = [vk::PipelineColorBlendAttachmentState::default()
            .blend_enable(false)
            .color_blend_op(vk::BlendOp::ADD)
            .alpha_blend_op(vk::BlendOp::ADD)
            .src_color_blend_factor(vk::BlendFactor::SRC_ALPHA)
            .dst_color_blend_factor(vk::BlendFactor::ONE_MINUS_SRC_ALPHA)
            .src_alpha_blend_factor(vk::BlendFactor::SRC_ALPHA)
            .dst_alpha_blend_factor(vk::BlendFactor::ONE_MINUS_SRC_ALPHA)
            .color_write_mask(vk::ColorComponentFlags::RGBA)];

        let color_blend_state = vk::PipelineColorBlendStateCreateInfo::default()
            .logic_op_enable(false)
            .logic_op(vk::LogicOp::COPY)
            .attachments(&color_blend_attachment_state);

        let depth_stencil_state = vk::PipelineDepthStencilStateCreateInfo::default()
            .depth_test_enable(true)
            .depth_write_enable(true)
            .depth_compare_op(vk::CompareOp::LESS)
            .depth_bounds_test_enable(false);

        let dynamic_state = vk::PipelineDynamicStateCreateInfo::default().dynamic_states(&[
            vk::DynamicState::VIEWPORT,
            vk::DynamicState::SCISSOR,
            vk::DynamicState::LINE_WIDTH,
        ]);

        let binding_descriptions = vec![vk::VertexInputBindingDescription {
            binding: 0,
            stride: std::mem::size_of::<Vertex>() as u32,
            input_rate: vk::VertexInputRate::VERTEX,
        }];

        let vertex_input_state = vk::PipelineVertexInputStateCreateInfo::default()
            .vertex_binding_descriptions(&binding_descriptions)
            .vertex_attribute_descriptions(&attribute_descriptions);

        let input_assembly_state = vk::PipelineInputAssemblyStateCreateInfo::default()
            .topology(vk::PrimitiveTopology::TRIANGLE_LIST)
            .primitive_restart_enable(false);

        let pipeline_layout_info =
            vk::PipelineLayoutCreateInfo::default().set_layouts(&descriptor_set_layouts);

        // Create the pipeline layout
        let pipeline_layout = unsafe {
            match device.create_pipeline_layout(&pipeline_layout_info, None) {
                Ok(layout) => layout,
                Err(err) => {
                    return Err(format!("Failed to create pipeline layout: {:?}", err));
                }
            }
        };

        let pipeline_create_info = vk::GraphicsPipelineCreateInfo::default()
            .stages(&shader_stages)
            .vertex_input_state(&vertex_input_state)
            .input_assembly_state(&input_assembly_state)
            .viewport_state(&viewport_state)
            .rasterization_state(&rasterization_state)
            .multisample_state(&multisample_state)
            .color_blend_state(&color_blend_state)
            .depth_stencil_state(&depth_stencil_state)
            .dynamic_state(&dynamic_state)
            .layout(pipeline_layout)
            .render_pass(renderpass.handle)
            .subpass(0)
            .base_pipeline_handle(vk::Pipeline::null())
            .base_pipeline_index(-1);

        // Create the graphics pipeline
        let graphics_pipeline = match unsafe {
            device.create_graphics_pipelines(
                vk::PipelineCache::null(),
                &[pipeline_create_info],
                None,
            )
        } {
            Ok(pipelines) => pipelines[0],
            Err(err) => {
                return Err(format!("Failed to create graphics pipeline: {:?}", err));
            }
        };

        Ok(VulkanPipeline {
            handle: graphics_pipeline,
            layout: pipeline_layout,
            render_pass: renderpass.handle,
            pipeline_type,
        })
    }

    pub fn destroy(&self, device: &ash::Device) {
        if self.handle == vk::Pipeline::null() {
            return;
        }

        unsafe {
            device.destroy_pipeline(self.handle, None);
            device.destroy_pipeline_layout(self.layout, None);
        }
    }

    pub fn bind(&self, device: &ash::Device, command_buffer: vk::CommandBuffer) {
        unsafe {
            device.cmd_bind_pipeline(
                command_buffer,
                match self.pipeline_type {
                    PipelineType::Graphics => vk::PipelineBindPoint::GRAPHICS,
                    PipelineType::Compute => vk::PipelineBindPoint::COMPUTE,
                },
                self.handle,
            );
        }
    }
}

impl<'backend> VulkanPipelineOperations<'backend> {
    pub fn create_graphics_pipeline(&mut self) -> Result<(), String> {
        let viewport = vk::Viewport {
            x: 0.0,
            y: self.backend.framebuffer_height as f32,
            width: self.backend.framebuffer_width as f32,
            height: self.backend.framebuffer_height as f32,
            min_depth: 0.0,
            max_depth: 1.0,
        };

        let scissor = vk::Rect2D {
            offset: vk::Offset2D { x: 0, y: 0 },
            extent: vk::Extent2D {
                width: self.backend.framebuffer_width,
                height: self.backend.framebuffer_height,
            },
        };

        // Attributes
        let attribute_descriptions = vec![
            // Position
            vk::VertexInputAttributeDescription {
                binding: 0,
                location: 0,
                format: vk::Format::R32G32B32_SFLOAT,
                offset: 0,
            },
        ];

        // Stages
        // NOTE: Should match the number of shader stages.
        let shader_stages = vec![
            // Vertex shader
            vk::PipelineShaderStageCreateInfo {
                stage: vk::ShaderStageFlags::VERTEX,
                module: self.backend.shader_modules[0].handle,
                p_name: self.backend.shader_modules[0].entry_point.as_ptr() as *const i8,
                ..Default::default()
            },
            // Fragment shader
            vk::PipelineShaderStageCreateInfo {
                stage: vk::ShaderStageFlags::FRAGMENT,
                module: self.backend.shader_modules[1].handle,
                p_name: self.backend.shader_modules[1].entry_point.as_ptr() as *const i8,
                ..Default::default()
            },
        ];

        // Descriptors

        self.backend.graphics_pipeline = Some(VulkanPipeline::new(
            device!(self.backend),
            PipelineType::Graphics,
            vec![viewport],
            vec![scissor],
            attribute_descriptions,
            vec![],
            shader_stages,
            main_renderpass!(self.backend),
            false,
        )?);

        Ok(())
    }

    pub fn destroy_graphics_pipeline(&mut self) -> Result<(), String> {
        if let Some(pipeline) = self.backend.graphics_pipeline.take() {
            pipeline.destroy(device!(self.backend));
        }
        self.backend.graphics_pipeline = None;
        Ok(())
    }
}
