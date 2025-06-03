//
// Created by Michael Desmedt on 30/05/2025.
//
#include "VulkanDescriptorSetLayoutManager.hpp"

#include "Core/Logger.hpp"
#include "Systems/RenderStateSystem.hpp"
#include "VulkanUtils.hpp"

namespace VoidArchitect
{
    namespace Platform
    {
        VulkanDescriptorSetLayoutManager::VulkanDescriptorSetLayoutManager(
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            const RenderStateInputLayout& sharedInputLayout)
            : m_Device(device->GetLogicalDeviceHandle()),
              m_Allocator(allocator),
              m_GlobalLayout(VK_NULL_HANDLE),
              m_PerMaterialLayout(VK_NULL_HANDLE),
              m_PerObjectLayout(VK_NULL_HANDLE)
        {
            if (sharedInputLayout.spaces.size() < 2)
            {
                VA_ENGINE_CRITICAL(
                    "[VulkanDescriptorSetLayoutManager] Invalid shared pipeline input resources "
                    "layout.");
                throw std::runtime_error("Invalid shared pipeline input resources layout.");
            }

            // NOTE By convention, space 0 is used for global resources, shared between, every
            //  graphics pipeline
            auto bindings = CreateDescriptorSetLayoutBindingsFromSpace(sharedInputLayout.spaces[0]);

            VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
            layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
            layoutCreateInfo.pBindings = bindings.data();

            VA_VULKAN_CHECK_RESULT_WARN(vkCreateDescriptorSetLayout(
                m_Device, &layoutCreateInfo, m_Allocator, &m_GlobalLayout));
            VA_ENGINE_TRACE("[VulkanDescriptorSetLayoutManager] Global layout created.");

            // NOTE By convention, space 1 is used for per-material resources, shared between, every
            //  graphics pipeline
            bindings = CreateDescriptorSetLayoutBindingsFromSpace(sharedInputLayout.spaces[1]);

            layoutCreateInfo = {};
            layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
            layoutCreateInfo.pBindings = bindings.data();

            VA_VULKAN_CHECK_RESULT_WARN(vkCreateDescriptorSetLayout(
                m_Device, &layoutCreateInfo, m_Allocator, &m_PerMaterialLayout));
            VA_ENGINE_TRACE("[VulkanDescriptorSetLayoutManager] Per-material layout created.");

            // TODO Implement space 2 for per-object resources
        }

        VulkanDescriptorSetLayoutManager::~VulkanDescriptorSetLayoutManager()
        {
            if (m_GlobalLayout != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorSetLayout(m_Device, m_GlobalLayout, m_Allocator);
                VA_ENGINE_TRACE("[VulkanDescriptorSetLayoutManager] Global layout destroyed.");
            }

            if (m_PerMaterialLayout != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorSetLayout(m_Device, m_PerMaterialLayout, m_Allocator);
                VA_ENGINE_TRACE(
                    "[VulkanDescriptorSetLayoutManager] Per-material layout destroyed.");
            }
        }

        std::vector<VkDescriptorSetLayoutBinding> VulkanDescriptorSetLayoutManager::
            CreateDescriptorSetLayoutBindingsFromSpace(const SpaceLayout& spaceLayout)
        {
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            for (uint32_t i = 0; i < spaceLayout.bindings.size(); ++i)
            {
                const auto& [type, binding, stage] = spaceLayout.bindings[i];

                VkDescriptorSetLayoutBinding layoutBinding{};
                layoutBinding.binding = binding;
                layoutBinding.descriptorType = TranslateEngineResourceTypeToVulkan(type);
                layoutBinding.descriptorCount = 1;
                layoutBinding.stageFlags = TranslateEngineShaderStageToVulkan(stage);

                bindings.push_back(layoutBinding);
            }

            return bindings;
        }
    } // namespace Platform
} // namespace VoidArchitect
