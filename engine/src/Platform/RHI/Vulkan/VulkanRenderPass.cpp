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
        CreateRenderPassFromConfig(config, swapchainFormat, depthFormat);
        VA_ENGINE_TRACE("[VulkanRenderpass] Renderpass '{}' created from config.", config.Name);
    }

    VulkanRenderPass::VulkanRenderPass(
        const std::unique_ptr<VulkanDevice>& device,
        const std::unique_ptr<VulkanSwapchain>& swapchain,
        VkAllocationCallbacks* allocator,
        int32_t x,
        int32_t y,
        uint32_t w,
        uint32_t h,
        float r,
        float g,
        float b,
        float a,
        float depth,
        uint32_t stencil)
        : IRenderPass("LegacyRenderPass"),
          m_Device(device->GetLogicalDeviceHandle()),
          m_Allocator(allocator),
          m_x(x),
          m_y(y),
          m_w(w),
          m_h(h),
          m_IsLegacy(true)
    {
        CreateLegacyRenderPass(swapchain, r, g, b, a, depth, stencil);
        VA_ENGINE_TRACE("[VulkanRenderpass] Legacy renderpass created.");
        // VA_ENGINE_TRACE("[VulkanRenderpass] Renderpass created.");
    }

    VulkanRenderPass::~VulkanRenderPass()
    {
        Release();
    }

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
        const Renderer::RenderPassConfig& config,
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
            attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            // Set final layout based on attachment type
            if (attachmentConfig.Name == "color")
            {
                attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                colorRefs.push_back(
                    {static_cast<uint32_t>(i), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
            }
            else if (attachmentConfig.Name == "depth")
            {
                attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                depthRef = {
                    static_cast<uint32_t>(i),
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                };
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
                if (attachmentConfig.Name == "depth")
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
        // TODO : Support multiple subpasses
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
    }

    void VulkanRenderPass::CreateLegacyRenderPass(
        const std::unique_ptr<VulkanSwapchain>& swapchain,
        const float r,
        const float g,
        const float b,
        const float a,
        const float depth,
        const uint32_t stencil)
    {
        auto mainSubpass = VkSubpassDescription{};
        mainSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        const std::vector attachments = {
            // Color attachment
            VkAttachmentDescription{
                .flags = 0,
                .format = swapchain->GetFormat(),
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
            // Depth attachment
            VkAttachmentDescription{
                .flags = 0,
                .format = swapchain->GetDepthFormat(),
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            }
        };
        const std::vector attachmentRefs = {
            // Color attachment
            VkAttachmentReference{
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            // Depth attachment
            VkAttachmentReference{
                .attachment = 1,
                .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            }
        };
        mainSubpass.colorAttachmentCount = 1;
        mainSubpass.pColorAttachments = &attachmentRefs[0];
        mainSubpass.pDepthStencilAttachment = &attachmentRefs[1];

        auto dependencies = VkSubpassDependency{};
        dependencies.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies.dstSubpass = 0;
        dependencies.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies.srcAccessMask = 0;
        dependencies.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        auto createInfo = VkRenderPassCreateInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = attachments.size();
        createInfo.pAttachments = attachments.data();
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &mainSubpass;
        createInfo.dependencyCount = 1;
        createInfo.pDependencies = &dependencies;

        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkCreateRenderPass(m_Device, &createInfo, m_Allocator, &m_Renderpass));

        m_ClearValues.emplace_back(VkClearValue{.color = {{r, g, b, a}}});
        m_ClearValues.emplace_back(VkClearValue{.depthStencil = {depth, stencil}});

        VA_ENGINE_TRACE("[VulkanRenderpass] Legacy renderpass created.");
    }
} // namespace VoidArchitect::Platform
