//
// Created by Michael Desmedt on 08/06/2025.
//
#pragma once

#include <vulkan/vulkan.h>

#include "Resources/Material.hpp"
#include "Systems/RenderStateSystem.hpp"

namespace VoidArchitect::Platform
{
    class VulkanDevice;
    class VulkanBuffer;

    class VulkanBindingGroupManager
    {
    public:
        static constexpr uint32_t MAX_MATERIALS = 1024;

        VulkanBindingGroupManager(
            std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator);
        ~VulkanBindingGroupManager();

        VulkanBindingGroupManager(const VulkanBindingGroupManager&) = delete;
        VulkanBindingGroupManager& operator=(const VulkanBindingGroupManager&) = delete;

        VkDescriptorSetLayout GetLayoutFor(const RenderStateConfig& stateConfig);

        void BindMaterialGroup(
            VkCommandBuffer cmdsBuf,
            MaterialHandle materialHandle,
            RenderStateHandle stateHandle);

        void UpdateMaterialUBO(
            MaterialHandle materialHandle,
            const Resources::MaterialUniformObject& ubo);

    private:
        static bool AreLayoutCompatible(MaterialHandle MaterialHandle, RenderStateHandle stateHandle);
        VkDescriptorSetLayout GetHandleFor(MaterialHandle materialHandle);
        VkDescriptorSet AllocateSet(VkDescriptorSetLayout layout);

        void UpdateDescriptorSet(VkDescriptorSet set, MaterialHandle materialHandle);
        bool NeedsUpdate(MaterialHandle materialHandle);
        void UpdateData(MaterialHandle materialHandle);

        std::unique_ptr<VulkanDevice>& m_Device;
        VkAllocationCallbacks* m_Allocator;

        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
        VAHashMap<size_t, VkDescriptorSetLayout> m_SetLayoutCache;
        VAHashMap<MaterialHandle, VkDescriptorSet> m_MaterialSetCache;
        VAHashMap<MaterialHandle, uint32_t> m_MaterialGenerations;

        std::unique_ptr<VulkanBuffer> m_MaterialUniformBuffer;
        void* m_MaterialUniformBufferMemory = nullptr;

        size_t m_MaterialUniformBufferSize = 0;
        size_t m_NextFreeUboOffset = 0;
        VAHashMap<MaterialHandle, size_t> m_MaterialUboOffsets;
    };

    inline std::unique_ptr<VulkanBindingGroupManager> g_VkBindingGroupManager;
}
