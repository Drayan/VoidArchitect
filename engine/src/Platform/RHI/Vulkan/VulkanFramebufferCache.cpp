//
// Created by Michael Desmedt on 09/06/2025.
//
#include "VulkanFramebufferCache.hpp"

#include "VulkanDevice.hpp"
#include "Core/Logger.hpp"

namespace VoidArchitect::Platform
{
    VulkanFramebufferCache::VulkanFramebufferCache(
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator)
        : m_Device(device),
          m_Allocator(allocator)
    {
    }

    VulkanFramebufferCache::~VulkanFramebufferCache()
    {
        Clear();
    }

    VkFramebuffer VulkanFramebufferCache::GetHandleFor(
        VkRenderPass renderPass,
        const VAArray<VkImageView>& attachments,
        const uint32_t width,
        const uint32_t height)
    {
        // First, look into the framebuffer cache
        const FramebufferCacheKey key{renderPass, attachments};
        if (m_FramebuffersCache.contains(key))
        {
            return m_FramebuffersCache[key];
        }

        // Otherwise, create a new framebuffer
        VkFramebuffer buffer;
        VkFramebufferCreateInfo framebufferCreateInfo = {};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.renderPass = renderPass;
        framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferCreateInfo.pAttachments = attachments.data();
        framebufferCreateInfo.width = width;
        framebufferCreateInfo.height = height;
        framebufferCreateInfo.layers = 1;

        vkCreateFramebuffer(
            m_Device->GetLogicalDeviceHandle(),
            &framebufferCreateInfo,
            m_Allocator,
            &buffer);
        m_FramebuffersCache[key] = buffer;

        VA_ENGINE_TRACE("[VulkanFramebufferCache] Framebuffer created.");

        return buffer;
    }

    void VulkanFramebufferCache::Clear()
    {
        for (auto& framebuffer : m_FramebuffersCache)
        {
            vkDestroyFramebuffer(
                m_Device->GetLogicalDeviceHandle(),
                framebuffer.second,
                m_Allocator);
            VA_ENGINE_TRACE("[VulkanFramebufferCache] Framebuffer destroyed.");
        }
        m_FramebuffersCache.clear();
    }

    size_t VulkanFramebufferCache::FramebufferCacheKeyHasher::operator()(
        const FramebufferCacheKey& key) const noexcept
    {
        size_t seed = 0;

        HashCombine(seed, std::get<0>(key));
        for (auto& view : std::get<1>(key))
        {
            HashCombine(seed, view);
        }

        return seed;
    }
}
