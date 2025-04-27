use ash::vk::*;
use std::fs::File;
use std::io::Read;
use std::path::Path;

use super::buffer::Vertex;

/// Holds Vulkan pipeline and layout handles.
pub struct VulkanPipeline {
    pub pipeline: Pipeline,
    pub layout: PipelineLayout,
}

impl VulkanPipeline {
    /// Create a new VulkanPipeline from pipeline and layout handles.
    pub fn new(pipeline: Pipeline, layout: PipelineLayout) -> Self {
        Self { pipeline, layout }
    }
}

/// Loads a SPIR-V shader file and creates a VkShaderModule.
/// # Arguments
/// * `device` - Vulkan logical device.
/// * `path` - Path to the .spv file.
/// # Returns
/// * `ShaderModule`
pub fn load_shader_module(device: &ash::Device, path: &Path) -> ShaderModule {
    let mut file =
        File::open(path).unwrap_or_else(|_| panic!("Failed to open shader file: {:?}", path));
    let mut code = Vec::new();
    file.read_to_end(&mut code)
        .unwrap_or_else(|_| panic!("Failed to read shader file: {:?}", path));
    let code_aligned =
        ash::util::read_spv(&mut std::io::Cursor::new(&code)).expect("Failed to parse SPIR-V");
    let create_info = ShaderModuleCreateInfo::default().code(&code_aligned);
    unsafe {
        device
            .create_shader_module(&create_info, None)
            .expect("Failed to create shader module")
    }
}

/// Creates a minimal pipeline layout (no descriptors/push constants).
pub fn create_pipeline_layout(device: &ash::Device) -> PipelineLayout {
    let layout_info = PipelineLayoutCreateInfo::default();
    unsafe {
        device
            .create_pipeline_layout(&layout_info, None)
            .expect("Failed to create pipeline layout")
    }
}

/// Creates a minimal graphics pipeline for a triangle.
/// # Arguments
/// * `device` - Vulkan logical device.
/// * `render_pass` - Existing VkRenderPass.
/// * `extent` - Swapchain extent (viewport/scissor).
/// * `shader_dir` - Path to the directory containing compiled SPIR-V shaders.
/// # Returns
/// * `VulkanPipeline` - Contains the pipeline and layout handles.
pub fn create_graphics_pipeline(
    device: &ash::Device,
    render_pass: RenderPass,
    extent: Extent2D,
    shader_dir: &Path,
) -> VulkanPipeline {
    // Load shaders
    let vert_path = shader_dir.join("triangle.vert.spv");
    let frag_path = shader_dir.join("triangle.frag.spv");
    let vert_module = load_shader_module(device, &vert_path);
    let frag_module = load_shader_module(device, &frag_path);

    let entry_point = std::ffi::CString::new("main").unwrap();

    // Create shader stages
    let shader_stages = [
        PipelineShaderStageCreateInfo::default()
            .stage(ShaderStageFlags::VERTEX)
            .module(vert_module)
            .name(&entry_point),
        PipelineShaderStageCreateInfo::default()
            .stage(ShaderStageFlags::FRAGMENT)
            .module(frag_module)
            .name(&entry_point),
    ];

    // Vertex input: position and color attributes
    let binding_description = [Vertex::get_binding_description()];
    let attribute_descriptions = Vertex::get_attribute_descriptions();
    let vertex_input_info = PipelineVertexInputStateCreateInfo::default()
        .vertex_binding_descriptions(&binding_description)
        .vertex_attribute_descriptions(&attribute_descriptions);

    // Input assembly: triangle list
    let input_assembly = PipelineInputAssemblyStateCreateInfo::default()
        .topology(PrimitiveTopology::TRIANGLE_LIST)
        .primitive_restart_enable(false);

    // Viewport and scissor (fixed)
    let viewport = Viewport {
        x: 0.0,
        y: 0.0,
        width: extent.width as f32,
        height: extent.height as f32,
        min_depth: 0.0,
        max_depth: 1.0,
    };
    let scissor = Rect2D {
        offset: Offset2D { x: 0, y: 0 },
        extent,
    };

    // Create arrays for viewport and scissor
    let viewports = [viewport];
    let scissors = [scissor];

    // Create viewport state
    let viewport_state =
        PipelineViewportStateCreateInfo::default().viewports(&viewports).scissors(&scissors);

    // Rasterizer: fill, no cull
    let rasterizer = PipelineRasterizationStateCreateInfo::default()
        .depth_clamp_enable(false)
        .rasterizer_discard_enable(false)
        .polygon_mode(PolygonMode::FILL)
        .cull_mode(CullModeFlags::NONE)
        .front_face(FrontFace::COUNTER_CLOCKWISE)
        .depth_bias_enable(false)
        .line_width(1.0);

    // Multisampling: disabled
    let multisampling = PipelineMultisampleStateCreateInfo::default()
        .sample_shading_enable(false)
        .rasterization_samples(SampleCountFlags::TYPE_1);

    // Color blend: opaque
    let color_blend_attachment = PipelineColorBlendAttachmentState::default()
        .color_write_mask(
            ColorComponentFlags::R
                | ColorComponentFlags::G
                | ColorComponentFlags::B
                | ColorComponentFlags::A,
        )
        .blend_enable(false);

    // Create array for color blend attachments
    let color_blend_attachments = [color_blend_attachment];

    // Create color blending state
    let color_blending =
        PipelineColorBlendStateCreateInfo::default().attachments(&color_blend_attachments);

    // Create pipeline layout
    let pipeline_layout = create_pipeline_layout(device);

    // Create pipeline info
    let pipeline_info = GraphicsPipelineCreateInfo::default()
        .stages(&shader_stages)
        .vertex_input_state(&vertex_input_info)
        .input_assembly_state(&input_assembly)
        .viewport_state(&viewport_state)
        .rasterization_state(&rasterizer)
        .multisample_state(&multisampling)
        .color_blend_state(&color_blending)
        .layout(pipeline_layout)
        .render_pass(render_pass)
        .subpass(0);

    // Create pipeline
    let pipelines = unsafe {
        device.create_graphics_pipelines(PipelineCache::null(), &[pipeline_info], None)
    }
    .expect("Failed to create graphics pipeline");

    // Cleanup shader modules
    unsafe {
        device.destroy_shader_module(vert_module, None);
        device.destroy_shader_module(frag_module, None);
    }

    VulkanPipeline::new(pipelines[0], pipeline_layout)
}
