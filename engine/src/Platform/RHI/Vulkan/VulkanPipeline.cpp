//
// Created by Michael Desmedt on 18/05/2025.
//
#include "VulkanPipeline.hpp"

#include "Core/Math/Mat4.hpp"
#include "Systems/PipelineSystem.hpp"
#include "VulkanDescriptorSetLayoutManager.hpp"
#include "VulkanDevice.hpp"
#include "VulkanRenderPass.hpp"
#include "VulkanRhi.hpp"
#include "VulkanShader.hpp"
#include "VulkanUtils.hpp"

namespace VoidArchitect::Platform
{
    // NOTE The Renderpass is currently completely hard coded, so we will just pass it from the RHI
    //  but it should comes from the config in the future.
    VulkanPipeline::VulkanPipeline(
        const PipelineConfig& config,
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        const std::unique_ptr<VulkanRenderPass>& renderPass)
        : m_Device(device->GetLogicalDeviceHandle()),
          m_Allocator(allocator)
    {
        // --- Dynamic state ---
        // NOTE We enable dynamic state for viewport and scissor, requiring to provide them in
        //  every frame. Therefore we just have to tell Vulkan that we use one viewport and one
        //  scissor.
        const std::vector dynamicState = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_LINE_WIDTH,
        };
        auto dynamicStateCreateInfo = VkPipelineDynamicStateCreateInfo{};
        dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicState.size());
        dynamicStateCreateInfo.pDynamicStates = dynamicState.data();

        auto viewportStateInfo = VkPipelineViewportStateCreateInfo{};
        viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportStateInfo.viewportCount = 1;
        viewportStateInfo.scissorCount = 1;

        // --- Attributes ---
        uint32_t offset = 0;
        std::vector<VkVertexInputAttributeDescription> attributes;
        for (uint32_t i = 0; i < config.vertexAttributes.size(); i++)
        {
            const auto& [type, format] = config.vertexAttributes[i];
            VkVertexInputAttributeDescription attribute{};
            attribute.location = i;
            attribute.binding = 0;
            attribute.format = TranslateEngineAttributeFormatToVulkanFormat(type, format);
            attribute.offset = offset;

            attributes.push_back(attribute);
            offset += GetEngineAttributeSize(type, format);
        }
        uint32_t stride = offset;

        // --- Vertex input binding ---
        auto bindingDescription = VkVertexInputBindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = stride;
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        // --- Vertex input ---
        auto vertexInputInfo = VkPipelineVertexInputStateCreateInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributes.data();

        // --- Input assembly --- TODO Should be configurable
        auto inputAssemblyInfo = VkPipelineInputAssemblyStateCreateInfo{};
        inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

        // --- Rasterizer ---
        auto rasterizerInfo = VkPipelineRasterizationStateCreateInfo{};
        rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizerInfo.depthClampEnable = VK_FALSE;
        rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
        rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizerInfo.lineWidth = 1.0f;
        rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizerInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizerInfo.depthBiasEnable = VK_FALSE;

        // --- Multisampling ---
        auto multisamplingInfo = VkPipelineMultisampleStateCreateInfo{};
        multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisamplingInfo.sampleShadingEnable = VK_FALSE;

        // --- Depth and stencil ---
        auto depthStencilInfo = VkPipelineDepthStencilStateCreateInfo{};
        depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilInfo.depthTestEnable = VK_TRUE;
        depthStencilInfo.depthWriteEnable = VK_TRUE;
        depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
        depthStencilInfo.stencilTestEnable = VK_FALSE;

        // --- Color blending ---
        auto colorBlendAttachment = VkPipelineColorBlendAttachmentState{};
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                              | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        auto colorBlendInfo = VkPipelineColorBlendStateCreateInfo{};
        colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendInfo.logicOpEnable = VK_FALSE;
        colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
        colorBlendInfo.attachmentCount = 1;
        colorBlendInfo.pAttachments = &colorBlendAttachment;

        // --- Descriptor sets ---
        m_DescriptorSetLayouts.resize(config.inputLayout.spaces.size());

        std::vector<VkDescriptorSetLayoutCreateInfo> descriptorSetLayoutsInfos;
        for (uint32_t i = 0; i < config.inputLayout.spaces.size(); i++)
        {
            const auto& space = config.inputLayout.spaces[i];
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            for (uint32_t j = 0; j < space.bindings.size(); j++)
            {
                const auto& [type, binding, stage] = space.bindings[j];

                VkDescriptorSetLayoutBinding descriptorSetLayoutBinding{};
                descriptorSetLayoutBinding.binding = binding;
                descriptorSetLayoutBinding.descriptorType =
                    TranslateEngineResourceTypeToVulkan(type);
                descriptorSetLayoutBinding.descriptorCount = 1;
                descriptorSetLayoutBinding.stageFlags = TranslateEngineShaderStageToVulkan(stage);

                bindings.push_back(descriptorSetLayoutBinding);
            }

            auto descriptorSetLayoutCreateInfo = VkDescriptorSetLayoutCreateInfo{};
            descriptorSetLayoutCreateInfo.sType =
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
            descriptorSetLayoutCreateInfo.pBindings = bindings.data();

            VA_VULKAN_CHECK_RESULT_WARN(vkCreateDescriptorSetLayout(
                m_Device, &descriptorSetLayoutCreateInfo, m_Allocator, &m_DescriptorSetLayouts[i]));

            VA_ENGINE_TRACE(
                "[VulkanPipeline] Descriptor set layout {} created, for pipeline '{}'.",
                i,
                config.name);
        }

        // --- Pipeline layout ---
        auto descriptorSetLayouts = std::vector<VkDescriptorSetLayout>{};
        descriptorSetLayouts.reserve(m_DescriptorSetLayouts.size() + 2);
        // Insert the global descriptor set layout
        descriptorSetLayouts.push_back(g_VkDescriptorSetLayoutManager->GetGlobalLayout());
        // Insert the material descriptor set layout
        descriptorSetLayouts.push_back(g_VkDescriptorSetLayoutManager->GetPerMaterialLayout());
        // Insert after that any custom descriptor set layout
        descriptorSetLayouts.insert(
            descriptorSetLayouts.end(),
            m_DescriptorSetLayouts.begin(),
            m_DescriptorSetLayouts.end());

        auto pipelineLayoutCreateInfo = VkPipelineLayoutCreateInfo{};
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount =
            static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();

        // --- Push constants ---
        auto pushConstantRange = VkPushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(Math::Mat4) * 2;

        pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
        pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

        VA_VULKAN_CHECK_RESULT_CRITICAL(vkCreatePipelineLayout(
            m_Device, &pipelineLayoutCreateInfo, m_Allocator, &m_PipelineLayout));
        VA_ENGINE_TRACE("[VulkanPipeline] Pipeline layout created.");

        // --- Pipeline ---
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        shaderStages.reserve(config.shaders.size());
        for (const auto& ishader : config.shaders)
        {
            const auto& shader = std::dynamic_pointer_cast<VulkanShader>(ishader);
            auto shaderStageInfo = shader->GetShaderStageInfo();
            shaderStages.push_back(shaderStageInfo);
        }

        // Store a reference to the shaders in the pipeline
        m_Shaders = config.shaders;

        auto pipelineCreateInfo = VkGraphicsPipelineCreateInfo{};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineCreateInfo.pStages = shaderStages.data();
        pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
        pipelineCreateInfo.pViewportState = &viewportStateInfo;
        pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
        pipelineCreateInfo.pInputAssemblyState = &inputAssemblyInfo;
        pipelineCreateInfo.pRasterizationState = &rasterizerInfo;
        pipelineCreateInfo.pMultisampleState = &multisamplingInfo;
        pipelineCreateInfo.pDepthStencilState = &depthStencilInfo;
        pipelineCreateInfo.pColorBlendState = &colorBlendInfo;
        pipelineCreateInfo.layout = m_PipelineLayout;
        pipelineCreateInfo.renderPass = renderPass->GetHandle();

        VA_VULKAN_CHECK_RESULT_CRITICAL(vkCreateGraphicsPipelines(
            m_Device, nullptr, 1, &pipelineCreateInfo, m_Allocator, &m_Pipeline));
        VA_ENGINE_TRACE("[VulkanPipeline] Pipeline {} created.", config.name);
    }

    VulkanPipeline::~VulkanPipeline()
    {
        // Release the shaders references
        m_Shaders.clear();

        if (m_DescriptorSetLayouts.size() > 0)
        {
            for (const auto& layout : m_DescriptorSetLayouts)
            {
                vkDestroyDescriptorSetLayout(m_Device, layout, m_Allocator);
                VA_ENGINE_TRACE("[VulkanPipeline] Descriptor set layout destroyed.");
            }
            m_DescriptorSetLayouts.clear();
        }

        if (m_PipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(m_Device, m_PipelineLayout, m_Allocator);
            VA_ENGINE_TRACE("[VulkanPipeline] Pipeline layout destroyed.");
        }
        if (m_Pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(m_Device, m_Pipeline, m_Allocator);
            VA_ENGINE_TRACE("[VulkanPipeline] Pipeline destroyed.");
        }
    }

    void VulkanPipeline::Bind(IRenderingHardware& rhi)
    {
        auto& vkRhi = dynamic_cast<VulkanRHI&>(rhi);
        vkCmdBindPipeline(
            vkRhi.GetCurrentCommandBuffer().GetHandle(),
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_Pipeline);
    }

    VkFormat VulkanPipeline::TranslateEngineAttributeFormatToVulkanFormat(
        const VertexAttributeType type, const AttributeFormat format)
    {
        auto vulkanFormat = VK_FORMAT_UNDEFINED;
        switch (type)
        {
            case VertexAttributeType::Float:
            {
                switch (format)
                {
                    case AttributeFormat::Float32:
                        vulkanFormat = VK_FORMAT_R32_SFLOAT;
                        break;
                }
            }
            break;

            case VertexAttributeType::Vec2:
            {
                switch (format)
                {
                    case AttributeFormat::Float32:
                        vulkanFormat = VK_FORMAT_R32G32_SFLOAT;
                        break;
                }
            }
            break;

            case VertexAttributeType::Vec3:
            {
                switch (format)
                {
                    case AttributeFormat::Float32:
                        vulkanFormat = VK_FORMAT_R32G32B32_SFLOAT;
                        break;
                }
            }
            break;

            case VertexAttributeType::Vec4:
            {
                switch (format)
                {
                    case AttributeFormat::Float32:
                        vulkanFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
                        break;
                }
            }
            break;

            default:
                break;
        }

        return vulkanFormat;
    }

    uint32_t VulkanPipeline::GetEngineAttributeSize(
        const VertexAttributeType type, const AttributeFormat format)
    {
        auto size = 0;
        switch (format)
        {
            case AttributeFormat::Float32:
                size = sizeof(float);
                break;

            default:
                break;
        }

        switch (type)
        {
            case VertexAttributeType::Float:
                size *= 1;
                break;
            case VertexAttributeType::Vec2:
                size *= 2;
                break;
            case VertexAttributeType::Vec3:
                size *= 3;
                break;
            case VertexAttributeType::Vec4:
                size *= 4;
                break;
            case VertexAttributeType::Mat4:
                size *= 16;
                break;
        }

        return size;
    }
} // namespace VoidArchitect::Platform
