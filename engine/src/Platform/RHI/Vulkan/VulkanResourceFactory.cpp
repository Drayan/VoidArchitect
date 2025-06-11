//
// Created by Michael Desmedt on 08/06/2025.
//
#include "VulkanResourceFactory.hpp"

#include "VulkanExecutionContext.hpp"
#include "VulkanMaterial.hpp"
#include "VulkanMesh.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanRenderPass.hpp"
#include "VulkanRenderTarget.hpp"
#include "VulkanShader.hpp"
#include "VulkanTexture.hpp"
#include "Core/Logger.hpp"
#include "Systems/ShaderSystem.hpp"

namespace VoidArchitect::Platform
{
    VkFormat TranslateEngineAttributeFormatToVulkan(
        const Renderer::AttributeType type,
        const Renderer::AttributeFormat format)
    {
        auto vulkanFormat = VK_FORMAT_UNDEFINED;
        switch (type)
        {
            case Renderer::AttributeType::Float:
            {
                switch (format)
                {
                    case Renderer::AttributeFormat::Float32:
                        vulkanFormat = VK_FORMAT_R32_SFLOAT;
                        break;
                }
            }
            break;

            case Renderer::AttributeType::Vec2:
            {
                switch (format)
                {
                    case Renderer::AttributeFormat::Float32:
                        vulkanFormat = VK_FORMAT_R32G32_SFLOAT;
                        break;
                }
            }
            break;

            case Renderer::AttributeType::Vec3:
            {
                switch (format)
                {
                    case Renderer::AttributeFormat::Float32:
                        vulkanFormat = VK_FORMAT_R32G32B32_SFLOAT;
                        break;
                }
            }
            break;

            case Renderer::AttributeType::Vec4:
            {
                switch (format)
                {
                    case Renderer::AttributeFormat::Float32:
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

    uint32_t GetEngineAttributeSize(
        const Renderer::AttributeType type,
        const Renderer::AttributeFormat format)
    {
        auto size = 0;
        switch (format)
        {
            case Renderer::AttributeFormat::Float32:
                size = sizeof(float);
                break;

            default:
                break;
        }

        switch (type)
        {
            case Renderer::AttributeType::Float:
                size *= 1;
                break;
            case Renderer::AttributeType::Vec2:
                size *= 2;
                break;
            case Renderer::AttributeType::Vec3:
                size *= 3;
                break;
            case Renderer::AttributeType::Vec4:
                size *= 4;
                break;
            case Renderer::AttributeType::Mat4:
                size *= 16;
                break;
        }

        return size;
    }

    VulkanResourceFactory::VulkanResourceFactory(
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator)
        : m_Device(device),
          m_Allocator(allocator)
    {
    }

    Resources::Texture2D* VulkanResourceFactory::CreateTexture2D(
        const std::string& name,
        const uint32_t width,
        const uint32_t height,
        const uint8_t channels,
        const bool hasTransparency,
        const VAArray<uint8_t>& data) const
    {
        return new VulkanTexture2D(
            m_Device,
            m_Allocator,
            name,
            width,
            height,
            channels,
            hasTransparency,
            data);
    }

    Resources::IRenderState* VulkanResourceFactory::CreateRenderState(
        const RenderStateConfig& config,
        const RenderPassHandle passHandle) const
    {
        // === 0. Gather dependencies ===
        // Ask the RenderPassSystem for the IRenderPass*
        const auto renderPassPtr = g_RenderPassSystem->GetPointerFor(passHandle);
        if (!renderPassPtr)
        {
            VA_ENGINE_ERROR("[VulkanRHI] Invalid render pass handle.");
            return nullptr;
        }
        const auto vkRenderPass = dynamic_cast<VulkanRenderPass*>(renderPassPtr);
        if (!vkRenderPass)
        {
            VA_ENGINE_ERROR("[VulkanRHI] Invalid render pass type.");
            return nullptr;
        }

        //TODO: Refactor ShaderSystem to use handle.

        // === 1. Construct fixed states (Rasterized, Depth, Blend, ...) ===
        // NOTE We enable dynamic state for viewport and scissor, requiring to provide them in
        //  every frame. Therefore we just have to tell Vulkan that we use one viewport and one
        //  scissor.
        const VAArray dynamicState = {
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

        auto rasterizerInfo = CreateRasterizerState(config);
        auto depthStencilInfo = CreateDepthStencilState(config);
        auto [colorBlendInfo, colorAttachmentInfo] = CreateColorBlendState(config);

        // --- Input assembly --- TODO Should be configurable
        auto inputAssemblyInfo = VkPipelineInputAssemblyStateCreateInfo{};
        inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

        // --- Multisampling ---
        auto multisamplingInfo = VkPipelineMultisampleStateCreateInfo{};
        multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisamplingInfo.sampleShadingEnable = VK_FALSE;

        // === 2. Construct the Vertex Input State ===
        auto [bindingDesc, attributeDescs] = GetVertexInputDesc(config);
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescs.
            size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescs.data();

        // === 3. Construct the Pipeline Layout ===
        VAArray<VkDescriptorSetLayout> descriptorSetLayouts;

        // Set 0: Global UBO (from ExecutionContext)
        descriptorSetLayouts.push_back(g_VkExecutionContext->GetGlobalSetLayout());

        // Set 1: Material UBO (from BindingGroupManager)
        descriptorSetLayouts.push_back(g_VkBindingGroupManager->GetLayoutFor(config));

        // TODO: More spaces ?

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();

        // TODO: Add push constants from the config
        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(Math::Mat4) * 2;

        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        VkPipelineLayout pipelineLayout;
        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkCreatePipelineLayout(m_Device->GetLogicalDeviceHandle(), &pipelineLayoutInfo,
                m_Allocator, &pipelineLayout));

        // === 4. Avenger assemble ! ===
        VAArray<VkPipelineShaderStageCreateInfo> shaderStages;
        shaderStages.reserve(config.shaders.size());
        for (const auto& shaderHandle : config.shaders)
        {
            const auto& shader = dynamic_cast<VulkanShader*>(g_ShaderSystem->GetPointerFor(
                shaderHandle));
            auto shaderStageInfo = shader->GetShaderStageInfo();
            shaderStages.push_back(shaderStageInfo);
        }

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.stageCount = static_cast<uint32_t>(config.shaders.size());
        pipelineCreateInfo.pStages = shaderStages.data();
        pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
        pipelineCreateInfo.pViewportState = &viewportStateInfo;
        pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
        pipelineCreateInfo.pInputAssemblyState = &inputAssemblyInfo;
        pipelineCreateInfo.pRasterizationState = &rasterizerInfo;
        pipelineCreateInfo.pMultisampleState = &multisamplingInfo;
        pipelineCreateInfo.pDepthStencilState = &depthStencilInfo;
        pipelineCreateInfo.pColorBlendState = &colorBlendInfo;
        pipelineCreateInfo.layout = pipelineLayout;
        pipelineCreateInfo.renderPass = vkRenderPass->GetHandle();
        pipelineCreateInfo.subpass = 0;

        VkPipeline pipeline;
        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkCreateGraphicsPipelines(m_Device->GetLogicalDeviceHandle(), VK_NULL_HANDLE, 1, &
                pipelineCreateInfo, m_Allocator, &pipeline));

        // === 5. Return the Pipeline ===
        return new VulkanPipeline(config.name, m_Device, m_Allocator, pipeline, pipelineLayout);
    }

    VkPipelineRasterizationStateCreateInfo VulkanResourceFactory::CreateRasterizerState(
        const RenderStateConfig& stateConfig) const
    {
        //TODO: Pull from the config
        auto rasterizerInfo = VkPipelineRasterizationStateCreateInfo{};
        rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizerInfo.depthClampEnable = VK_FALSE;
        rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
        rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizerInfo.lineWidth = 1.0f;
        rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizerInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizerInfo.depthBiasEnable = VK_FALSE;

        return rasterizerInfo;
    }

    VkPipelineDepthStencilStateCreateInfo VulkanResourceFactory::CreateDepthStencilState(
        const RenderStateConfig& stateConfig) const
    {
        //TODO: Pull from the config.
        auto depthStencilInfo = VkPipelineDepthStencilStateCreateInfo{};
        depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilInfo.depthTestEnable = VK_TRUE;
        depthStencilInfo.depthWriteEnable = VK_TRUE;
        depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
        depthStencilInfo.stencilTestEnable = VK_FALSE;

        return depthStencilInfo;
    }

    std::pair<VkPipelineColorBlendStateCreateInfo, VkPipelineColorBlendAttachmentState>
    VulkanResourceFactory::CreateColorBlendState(const RenderStateConfig& stateConfig) const
    {
        //TODO: Pull this from config.
        auto retVlt = std::pair<VkPipelineColorBlendStateCreateInfo,
                                VkPipelineColorBlendAttachmentState>{};
        auto& colorBlendAttachment = retVlt.second;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        auto& colorBlendInfo = retVlt.first;
        colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendInfo.logicOpEnable = VK_FALSE;
        colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
        colorBlendInfo.attachmentCount = 1;
        colorBlendInfo.pAttachments = &colorBlendAttachment;

        return retVlt;
    }

    std::pair<VkVertexInputBindingDescription, std::vector<VkVertexInputAttributeDescription>>
    VulkanResourceFactory::GetVertexInputDesc(const RenderStateConfig& stateConfig) const
    {
        uint32_t offset = 0;
        VAArray<VkVertexInputAttributeDescription> attributeDescs;
        for (uint32_t i = 0; i < stateConfig.vertexAttributes.size(); i++)
        {
            const auto& [type, format] = stateConfig.vertexAttributes[i];
            VkVertexInputAttributeDescription attribute{};
            attribute.location = i;
            attribute.binding = 0;
            attribute.format = TranslateEngineAttributeFormatToVulkan(type, format);
            attribute.offset = offset;

            attributeDescs.push_back(attribute);
            offset += GetEngineAttributeSize(type, format);
        }
        const uint32_t stride = offset;

        // --- Vertex input binding ---
        auto bindingDescription = VkVertexInputBindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = stride;
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return std::pair(bindingDescription, attributeDescs);
    }

    Resources::IMaterial* VulkanResourceFactory::CreateMaterial(
        const std::string& name,
        const MaterialTemplate& templ) const
    {
        return new VulkanMaterial(name, templ);
    }

    Resources::IShader* VulkanResourceFactory::CreateShader(
        const std::string& name,
        const ShaderConfig& config,
        const VAArray<uint8_t>& data) const
    {
        return new VulkanShader(m_Device, m_Allocator, name, config, data);
    }

    Resources::IMesh* VulkanResourceFactory::CreateMesh(
        const std::string& name,
        const VAArray<Resources::MeshVertex>& vertices,
        const VAArray<uint32_t>& indices) const
    {
        return new VulkanMesh(m_Device, m_Allocator, name, vertices, indices);
    }

    Resources::IRenderTarget* VulkanResourceFactory::CreateRenderTarget(
        const Renderer::RenderTargetConfig& config) const
    {
        VkImageUsageFlags usageFlags = 0;
        VkImageAspectFlags aspectFlags = 0;

        if (static_cast<uint32_t>(config.usage) & static_cast<uint32_t>(
            Renderer::RenderTargetUsage::ColorAttachment))
        {
            usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            aspectFlags |= VK_IMAGE_ASPECT_COLOR_BIT;
        }
        if (static_cast<uint32_t>(config.usage) & static_cast<uint32_t>(
            Renderer::RenderTargetUsage::DepthStencilAttachment))
        {
            usageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            aspectFlags |= VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        if (static_cast<uint32_t>(config.usage) & static_cast<uint32_t>(
            Renderer::RenderTargetUsage::RenderTexture))
        {
            usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        }
        // usageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        uint32_t width = config.width;
        uint32_t height = config.height;

        if (config.sizingPolicy == Renderer::RenderTargetConfig::SizingPolicy::RelativeToViewport)
        {
            //TODO Get swapchain width and height
            width = 1280;
            height = 720;
        }

        auto image = VulkanImage(
            m_Device,
            m_Allocator,
            width,
            height,
            TranslateEngineTextureFormatToVulkan(config.format),
            aspectFlags,
            VK_IMAGE_TILING_OPTIMAL,
            usageFlags,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        return new VulkanRenderTarget(config.name, std::move(image));
    }

    Resources::IRenderTarget* VulkanResourceFactory::CreateRenderTarget(
        const std::string& name,
        const VkImage nativeImage,
        const VkFormat format) const
    {
        auto image = VulkanImage(
            m_Device,
            m_Allocator,
            nativeImage,
            format,
            VK_IMAGE_ASPECT_COLOR_BIT);

        return new VulkanRenderTarget(name, std::move(image));
    }

    Resources::IRenderPass* VulkanResourceFactory::CreateRenderPass(
        const Renderer::RenderPassConfig& config,
        const Renderer::PassPosition passPosition,
        const VkFormat swapchainFormat,
        const VkFormat depthFormat) const
    {
        return new VulkanRenderPass(
            config,
            m_Device,
            m_Allocator,
            passPosition,
            swapchainFormat,
            depthFormat);
    }
}
