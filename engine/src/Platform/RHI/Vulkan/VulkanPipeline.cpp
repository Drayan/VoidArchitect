//
// Created by Michael Desmedt on 18/05/2025.
//
#include "VulkanPipeline.hpp"

#include "VulkanDevice.hpp"
#include "VulkanRenderpass.hpp"
#include "VulkanShader.hpp"
#include "VulkanUtils.hpp"

namespace VoidArchitect::Platform
{
    VulkanPipeline::VulkanPipeline(
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        const std::unique_ptr<VulkanRenderpass>& renderPass,
        const std::vector<VulkanShader>& shaders,
        const std::vector<VkVertexInputAttributeDescription>& attributes,
        const std::vector<VkDescriptorSetLayout>& descriptorSets)
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

        // --- Vertex input binding ---
        auto bindingDescription = VkVertexInputBindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = 12;
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
        rasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
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

        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        auto colorBlendInfo = VkPipelineColorBlendStateCreateInfo{};
        colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendInfo.logicOpEnable = VK_FALSE;
        colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
        colorBlendInfo.attachmentCount = 1;
        colorBlendInfo.pAttachments = &colorBlendAttachment;

        // --- Pipeline layout ---
        auto pipelineLayoutCreateInfo = VkPipelineLayoutCreateInfo{};
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSets.size());
        pipelineLayoutCreateInfo.pSetLayouts = descriptorSets.data();

        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkCreatePipelineLayout(m_Device, &pipelineLayoutCreateInfo, m_Allocator, &
                m_PipelineLayout));
        VA_ENGINE_TRACE("[VulkanPipeline] Pipeline layout created.");

        // --- Pipeline ---
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        shaderStages.reserve(shaders.size());
        for (const auto& shader : shaders)
        {
            auto shaderStageInfo = shader.GetShaderStageInfo();
            shaderStages.push_back(shaderStageInfo);
        }

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

        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkCreateGraphicsPipelines(m_Device, nullptr, 1, &pipelineCreateInfo, m_Allocator, &
                m_Pipeline));
        VA_ENGINE_TRACE("[VulkanPipeline] Pipeline created.");
    }

    VulkanPipeline::~VulkanPipeline()
    {
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

    void VulkanPipeline::Bind(
        const VulkanCommandBuffer& cmdBuf,
        const VkPipelineBindPoint bindPoint) const
    {
        vkCmdBindPipeline(cmdBuf.GetHandle(), bindPoint, m_Pipeline);
    }
} // VoidArchitect
