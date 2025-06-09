//
// Created by Michael Desmedt on 30/05/2025.
//
#pragma once

#include "VulkanDevice.hpp"

#include <vulkan/vulkan.h>

#include "Systems/RenderStateSystem.hpp"

namespace VoidArchitect
{
    struct SpaceLayout;

    namespace Platform
    {
        class VulkanDevice;

        class VulkanDescriptorSetLayoutManager
        {
        public:
            VulkanDescriptorSetLayoutManager(
                const std::unique_ptr<VulkanDevice>& device,
                VkAllocationCallbacks* allocator,
                const Renderer::RenderStateInputLayout& sharedInputLayout);
            ~VulkanDescriptorSetLayoutManager();

            static VAArray<VkDescriptorSetLayoutBinding> CreateDescriptorSetLayoutBindingsFromSpace(
                const Renderer::SpaceLayout& spaceLayout);

            [[nodiscard]] VkDescriptorSetLayout GetGlobalLayout() const { return m_GlobalLayout; }

            [[nodiscard]] VkDescriptorSetLayout GetPerMaterialLayout() const
            {
                return m_PerMaterialLayout;
            }

            [[nodiscard]] VkDescriptorSetLayout GetPerObjectLayout() const
            {
                return m_PerObjectLayout;
            }

            [[nodiscard]] const Renderer::RenderStateInputLayout& GetSharedInputLayout() const
            {
                return m_SharedInputLayout;
            }

        private:
            Renderer::RenderStateInputLayout m_SharedInputLayout;

            VkDevice m_Device;
            VkAllocationCallbacks* m_Allocator;

            VkDescriptorSetLayout m_GlobalLayout;
            VkDescriptorSetLayout m_PerMaterialLayout;
            VkDescriptorSetLayout m_PerObjectLayout;

            // NOTE We could cache the layouts here and reuse them if they are the same.
        };

        inline std::unique_ptr<VulkanDescriptorSetLayoutManager> g_VkDescriptorSetLayoutManager;
    } // namespace Platform
} // namespace VoidArchitect
