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
        const RenderPassConfig& config,
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        Renderer::PassPosition passPosition,
        VkFormat swapchainFormat,
        VkFormat depthFormat)
        : IRenderPass(config.Name),
          m_Device(device->GetLogicalDeviceHandle()),
          m_Allocator(allocator),
          m_Config(config),
          m_x(0),
          m_y(0),
          m_w(0),
          m_h(0)

    {
        CreateRenderPassFromConfig(config, passPosition, swapchainFormat, depthFormat);
    }

    VulkanRenderPass::~VulkanRenderPass() { Release(); }

    void VulkanRenderPass::Begin(IRenderingHardware& rhi, const Resources::RenderTargetPtr& target)
    {
        auto& vulkanRhi = dynamic_cast<VulkanRHI&>(rhi);
        const auto vulkanTarget = std::dynamic_pointer_cast<VulkanRenderTarget>(target);

        if (!vulkanTarget)
        {
            VA_ENGINE_ERROR(
                "[VulkanRenderpass] Invalid render target for renderpass '{}'.",
                m_Name);
            return;
        }

        // Store current target for End() and compatibility checks
        m_CurrentTarget = vulkanTarget;

        // Update dimensions from target if not legacy
        if (!m_IsLegacy)
        {
            m_w = vulkanTarget->GetWidth();
            m_h = vulkanTarget->GetHeight();
        }

        // Call legacy Begin() method
        const auto imageIndex = vulkanTarget->IsMainTarget() ? vulkanRhi.GetImageIndex() : 0;

        if (!vulkanTarget->HasValidFramebuffers() && vulkanTarget->IsMainTarget())
        {
            // For main targets, we need to create framebuffers
            const auto& swapchain = vulkanRhi.GetSwapchainRef();
            const auto imageCount = swapchain->GetImageCount();

            VA_ENGINE_TRACE(
                "[VulkanRenderpass] Creating {} framebuffers for main target '{}'.",
                imageCount,
                vulkanTarget->GetName());

            for (uint32_t i = 0; i < imageCount; i++)
            {
                const std::vector attachments = {
                    swapchain->GetSwapchainImage(i).GetView(),
                    swapchain->GetDepthImage().GetView()
                };

                vulkanTarget->CreateFramebufferForImage(m_Renderpass, attachments, i);
            }
        }

        Begin(vulkanRhi.GetCurrentCommandBuffer(), vulkanTarget->GetFramebuffer(imageIndex));
    }

    void VulkanRenderPass::Begin(VulkanCommandBuffer& cmdBuf, const VkFramebuffer framebuffer) const
    {
        auto beginInfo = VkRenderPassBeginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.renderPass = m_Renderpass;
        beginInfo.framebuffer = framebuffer;
        beginInfo.renderArea.offset = {m_x, m_y};
        beginInfo.renderArea.extent = {m_w, m_h};
        beginInfo.clearValueCount = m_ClearValues.size();
        beginInfo.pClearValues = m_ClearValues.data();

        vkCmdBeginRenderPass(cmdBuf.GetHandle(), &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
        cmdBuf.SetState(CommandBufferState::InRenderpass);
    }

    void VulkanRenderPass::End(IRenderingHardware& rhi)
    {
        auto& vulkanRhi = dynamic_cast<VulkanRHI&>(rhi);

        End(vulkanRhi.GetCurrentCommandBuffer());

        m_CurrentTarget = nullptr;
    }

    void VulkanRenderPass::End(VulkanCommandBuffer& cmdBuf) const
    {
        vkCmdEndRenderPass(cmdBuf.GetHandle());
        cmdBuf.SetState(CommandBufferState::Recording);
    }

    bool VulkanRenderPass::IsCompatibleWith(const Resources::RenderTargetPtr& target) const
    {
        if (!target)
            return false;

        // For legacy render passes, always compatible for backwards compatibility
        if (m_IsLegacy)
            return true;

        auto vulkanTarget = std::dynamic_pointer_cast<VulkanRenderTarget>(target);
        if (!vulkanTarget)
            return false;

        // Check dimensions (could be relaxed in the future)
        if (m_w > 0 && m_h > 0)
        {
            if (vulkanTarget->GetWidth() != m_w || vulkanTarget->GetHeight() != m_h)
            {
                VA_ENGINE_DEBUG(
                    "[VulkanRenderPass] Dimensions mismatch: {}x{} vs {}x{}.",
                    vulkanTarget->GetWidth(),
                    vulkanTarget->GetHeight(),
                    m_w,
                    m_h);
                return false;
            }
        }

        // TODO: Add format compatibility checks when we store expected formats
        //  For now, assume compatibility if dimensions match;

        return true;
    }

    void VulkanRenderPass::Release()
    {
        if (m_Renderpass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(m_Device, m_Renderpass, m_Allocator);
            m_Renderpass = VK_NULL_HANDLE;
            VA_ENGINE_TRACE("[VulkanRenderPass] Renderpass '{}' released.", m_Name);
        }
    }

    void VulkanRenderPass::CreateRenderPassFromConfig(
        const RenderPassConfig& config,
        Renderer::PassPosition passPosition,
        VkFormat swapchainFormat,
        VkFormat depthFormat)
    {
        std::vector<VkAttachmentDescription> attachments;
        std::vector<VkAttachmentReference> colorRefs;
        std::optional<VkAttachmentReference> depthRef;

        // Create attachments from config
        for (size_t i = 0; i < config.Attachments.size(); i++)
        {
            const auto& attachmentConfig = config.Attachments[i];

            VkAttachmentDescription attachment = {};
            attachment.flags = 0;

            // Translate format, with special handling for swapchain/depth
            if (attachmentConfig.Format == Renderer::TextureFormat::SWAPCHAIN_FORMAT)
            {
                attachment.format = swapchainFormat;
            }
            else if (attachmentConfig.Format == Renderer::TextureFormat::SWAPCHAIN_DEPTH)
            {
                attachment.format = depthFormat;
            }
            else
            {
                attachment.format = TranslateEngineTextureFormatToVulkan(attachmentConfig.Format);
            }

            attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp = TranslateEngineLoadOpToVulkan(attachmentConfig.LoadOp);
            attachment.storeOp = TranslateEngineStoreOpToVulkan(attachmentConfig.StoreOp);
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            bool isDepthAttachment = false;

            if (attachmentConfig.Name == "depth"
                || attachmentConfig.Format == Renderer::TextureFormat::SWAPCHAIN_DEPTH
                || attachmentConfig.Format == Renderer::TextureFormat::D32_SFLOAT
                || attachmentConfig.Format == Renderer::TextureFormat::D24_UNORM_S8_UINT)
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
            else if (
                attachmentConfig.Name == "color"
                || attachmentConfig.Format == Renderer::TextureFormat::SWAPCHAIN_FORMAT)
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
            if (attachmentConfig.LoadOp == Renderer::LoadOp::Clear)
            {
                VkClearValue clearValue = {};
                if (isDepthAttachment)
                {
                    clearValue.depthStencil = {
                        attachmentConfig.ClearDepth,
                        attachmentConfig.ClearStencil
                    };
                }
                else
                {
                    clearValue.color = {
                        {
                            attachmentConfig.ClearColor.X(),
                            attachmentConfig.ClearColor.Y(),
                            attachmentConfig.ClearColor.Z(),
                            attachmentConfig.ClearColor.W()
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
        dependency.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
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
            config.Name,
            colorRefs.size(),
            depthRef.has_value() ? 1 : 0);
    }
} // namespace VoidArchitect::Platform
