//
// Created by Michael Desmedt on 09/06/2025.
//
#pragma once
#include <vulkan/vulkan_core.h>

namespace VoidArchitect
{
    namespace Platform
    {
        class VulkanDevice;

        class VulkanFramebufferCache
        {
        public:
            explicit VulkanFramebufferCache(
                const std::unique_ptr<VulkanDevice>& device,
                VkAllocationCallbacks* allocator);
            ~VulkanFramebufferCache();

            VulkanFramebufferCache(const VulkanFramebufferCache&) = delete;
            VulkanFramebufferCache& operator=(const VulkanFramebufferCache&) = delete;
            VulkanFramebufferCache(VulkanFramebufferCache&&) = delete;
            VulkanFramebufferCache& operator=(VulkanFramebufferCache&&) = delete;

            VkFramebuffer GetHandleFor(
                VkRenderPass renderPass,
                const VAArray<VkImageView>& attachments,
                uint32_t width,
                uint32_t height);

            void Clear();

        private:
            const std::unique_ptr<VulkanDevice>& m_Device;
            VkAllocationCallbacks* m_Allocator;

            using FramebufferCacheKey = std::tuple<VkRenderPass, VAArray<VkImageView>>;

            struct FramebufferCacheKeyHasher
            {
                size_t operator()(const FramebufferCacheKey& key) const noexcept;
            };

            VAHashMap<FramebufferCacheKey, VkFramebuffer, FramebufferCacheKeyHasher>
            m_FramebuffersCache;
        };
    } // Platform
} // VoidArchitect
