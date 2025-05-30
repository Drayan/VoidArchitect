//
// Created by Michael Desmedt on 30/05/2025.
//
#pragma once

#include "VulkanDevice.hpp"


#include <vulkan/vulkan.h>

namespace VoidArchitect
{
    struct SpaceLayout;
    struct PipelineInputLayout;

    namespace Platform
    {
        class VulkanDevice;

        class VulkanDescriptorSetLayoutManager
        {
        public:
            VulkanDescriptorSetLayoutManager(
                const std::unique_ptr<VulkanDevice>& device,
                VkAllocationCallbacks* allocator,
                const PipelineInputLayout& sharedInputLayout);
            ~VulkanDescriptorSetLayoutManager();

            static std::vector<VkDescriptorSetLayoutBinding>
            CreateDescriptorSetLayoutBindingsFromSpace(const SpaceLayout& spaceLayout);

            VkDescriptorSetLayout GetGlobalLayout() const { return m_GlobalLayout; }
            VkDescriptorSetLayout GetPerMaterialLayout() const { return m_PerMaterialLayout; }
            VkDescriptorSetLayout GetPerObjectLayout() const { return m_PerObjectLayout; }

        private:
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
