//
// Created by Michael Desmedt on 19/05/2025.
//
#include "VulkanRenderPass.hpp"

#include "Core/Logger.hpp"
#include "VulkanDevice.hpp"
#include "VulkanSwapchain.hpp"
#include "VulkanUtils.hpp"

namespace VoidArchitect::Platform
{
    VulkanRenderPass::VulkanRenderPass(
        const Renderer::RenderPassConfig& config,
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        Renderer::PassPosition passPosition,
        VkFormat swapchainFormat,
        VkFormat depthFormat)
        : IRenderPass(config.name, {}),
          m_Device(device->GetLogicalDeviceHandle()),
          m_Allocator(allocator),
          m_x(0),
          m_y(0),
          m_w(0),
          m_h(0)

    {
        VAArray<Renderer::TextureFormat> colorAttachmentFormats;
        std::optional<Renderer::TextureFormat> depthAttachmentFormat;
        for (auto attachment : config.attachments)
        {
            if (attachment.name == "color")
            {
                colorAttachmentFormats.push_back(attachment.format);
            }
            else if (attachment.name == "depth")
            {
                depthAttachmentFormat = attachment.format;
            }
        }
        m_Signature = {colorAttachmentFormats, depthAttachmentFormat};

        CreateRenderPassFromConfig(config, passPosition, swapchainFormat, depthFormat);
    }

    VulkanRenderPass::~VulkanRenderPass()
    {
        if (m_Renderpass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(m_Device, m_Renderpass, m_Allocator);
            VA_ENGINE_TRACE("[VulkanRenderpass] Renderpass destroyed.");
        }
    }

    void VulkanRenderPass::CreateRenderPassFromConfig(
        const Renderer::RenderPassConfig& config,
        Renderer::PassPosition passPosition,
        VkFormat swapchainFormat,
        VkFormat depthFormat)
    {
        VAArray<VkAttachmentDescription> attachments;
        VAArray<VkAttachmentReference> colorRefs;
        std::optional<VkAttachmentReference> depthRef;

        // Create attachments from config
        for (size_t i = 0; i < config.attachments.size(); i++)
        {
            const auto& attachmentConfig = config.attachments[i];

            VkAttachmentDescription attachment = {};
            attachment.flags = 0;

            // Translate format, with special handling for swapchain/depth
            if (attachmentConfig.format == Renderer::TextureFormat::SWAPCHAIN_FORMAT)
            {
                attachment.format = swapchainFormat;
            }
            else if (attachmentConfig.format == Renderer::TextureFormat::SWAPCHAIN_DEPTH)
            {
                attachment.format = depthFormat;
            }
            else
            {
                attachment.format = TranslateEngineTextureFormatToVulkan(attachmentConfig.format);
            }

            attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp = TranslateEngineLoadOpToVulkan(attachmentConfig.loadOp);
            attachment.storeOp = TranslateEngineStoreOpToVulkan(attachmentConfig.storeOp);
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            bool isDepthAttachment = false;

            if (attachmentConfig.name == "depth" || attachmentConfig.format ==
                Renderer::TextureFormat::SWAPCHAIN_DEPTH || attachmentConfig.format ==
                Renderer::TextureFormat::D32_SFLOAT || attachmentConfig.format ==
                Renderer::TextureFormat::D24_UNORM_S8_UINT)
            {
                isDepthAttachment = true;
                depthRef = {
                    static_cast<uint32_t>(i),
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                };
                switch (passPosition)
                {
                    case Renderer::PassPosition::First:
                    case Renderer::PassPosition::Standalone:
                        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                        attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                        break;
                    case Renderer::PassPosition::Last:
                    case Renderer::PassPosition::Middle:
                        attachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                        attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                        break;
                }
            }
            else if (attachmentConfig.name == "color" || attachmentConfig.format ==
                Renderer::TextureFormat::SWAPCHAIN_FORMAT)
            {
                colorRefs.push_back(
                    {static_cast<uint32_t>(i), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
                switch (passPosition)
                {
                    case Renderer::PassPosition::Standalone:
                        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                        break;
                    case Renderer::PassPosition::First:
                        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                        attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                        break;
                    case Renderer::PassPosition::Middle:
                        attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                        attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                        break;
                    case Renderer::PassPosition::Last:
                        attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                        break;
                }
            }
            else
            {
                // TODO: We should verify that these settings are correct
                attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                colorRefs.push_back(
                    {static_cast<uint32_t>(i), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            }

            attachments.push_back(attachment);

            // Setup clear values
            if (attachmentConfig.loadOp == Renderer::LoadOp::Clear)
            {
                VkClearValue clearValue = {};
                if (isDepthAttachment)
                {
                    clearValue.depthStencil = {
                        attachmentConfig.clearDepth,
                        attachmentConfig.clearStencil
                    };
                }
                else
                {
                    clearValue.color = {
                        {
                            attachmentConfig.clearColor.X(),
                            attachmentConfig.clearColor.Y(),
                            attachmentConfig.clearColor.Z(),
                            attachmentConfig.clearColor.W()
                        }
                    };
                }
                m_ClearValues.push_back(clearValue);
            }
        }

        // Create subpass (assuming single subpass for now)
        // TODO : Support subpasses optimization
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
        subpass.pColorAttachments = colorRefs.empty() ? nullptr : colorRefs.data();
        subpass.pDepthStencilAttachment = depthRef.has_value() ? &depthRef.value() : nullptr;

        // Subpass dependencies
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        // if (depthRef.has_value())
        // {
        //     dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        //     dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        //     dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        // }

        // Create render pass
        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkCreateRenderPass(m_Device, &renderPassInfo, m_Allocator, &m_Renderpass));

        VA_ENGINE_TRACE(
            "[VulkanRenderpass] Renderpass '{}' created from config, with {} color attachments and "
            "{} depth attachment.",
            config.name,
            colorRefs.size(),
            depthRef.has_value() ? 1 : 0);
    }
} // namespace VoidArchitect::Platform
