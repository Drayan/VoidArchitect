//
// Created by Michael Desmedt on 19/05/2025.
//
#include "VulkanRenderpass.hpp"

#include "VulkanDevice.hpp"
#include "VulkanSwapchain.hpp"
#include "Core/Logger.hpp"

namespace VoidArchitect::Platform
{
    VulkanRenderpass::VulkanRenderpass(
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
        : m_Device(device->GetLogicalDeviceHandle()),
          m_Allocator(allocator),
          m_x(x),
          m_y(y),
          m_w(w),
          m_h(h)
    {
        auto mainSubpass = VkSubpassDescription{};
        mainSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        // Attachments TODO Should be configurable
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

        // Renderpass dependencies. TODO Should be configurable
        auto dependencies = VkSubpassDependency{};
        dependencies.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies.dstSubpass = 0;
        dependencies.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies.srcAccessMask = 0;
        dependencies.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        auto createInfo = VkRenderPassCreateInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = attachments.size();
        createInfo.pAttachments = attachments.data();
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &mainSubpass;
        createInfo.dependencyCount = 1;
        createInfo.pDependencies = &dependencies;

        if (vkCreateRenderPass(m_Device, &createInfo, m_Allocator, &m_Renderpass) != VK_SUCCESS)
        {
            VA_ENGINE_CRITICAL("[VulkanRenderpass] Failed to create a renderpass.");
            return;
        }

        m_ClearValues.emplace_back(VkClearValue{.color = {r, g, b, a}});
        m_ClearValues.emplace_back(VkClearValue{.depthStencil = {depth, stencil}});

        VA_ENGINE_TRACE("[VulkanRenderpass] Renderpass created.");
    }

    VulkanRenderpass::~VulkanRenderpass()
    {
        if (m_Renderpass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(m_Device, m_Renderpass, m_Allocator);
            VA_ENGINE_TRACE("[VulkanRenderpass] Renderpass destroyed.");
        }
    }

    void VulkanRenderpass::Begin(VulkanCommandBuffer& cmdBuf, const VkFramebuffer framebuffer) const
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

    void VulkanRenderpass::End(VulkanCommandBuffer& cmdBuf) const
    {
        vkCmdEndRenderPass(cmdBuf.GetHandle());
        cmdBuf.SetState(CommandBufferState::Recording);
    }
} // VoidArchitect
