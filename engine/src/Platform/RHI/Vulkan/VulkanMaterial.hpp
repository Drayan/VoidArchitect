//
// Created by Michael Desmedt on 20/05/2025.
//
#pragma once
#include "Platform/RHI/Material.hpp"

#include <vulkan/vulkan.h>

namespace VoidArchitect::Platform
{
    class VulkanRHI;
    class VulkanShader;
    class VulkanDevice;
    class VulkanSwapchain;
    class VulkanRenderpass;
    class VulkanPipeline;
    class VulkanBuffer;

    class VulkanMaterial : public IMaterial
    {
    public:
        constexpr static size_t MAX_INSTANCES = 1024;

        VulkanMaterial(
            const VulkanRHI& rhi,
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            const std::unique_ptr<VulkanSwapchain>& swapchain,
            const std::unique_ptr<VulkanRenderpass>& renderpass);
        ~VulkanMaterial() override;

        void Use(IRenderingHardware& rhi) override;

        void SetGlobalUniforms(
            IRenderingHardware& rhi,
            const Math::Mat4& projection,
            const Math::Mat4& view) override;
        void SetObject(IRenderingHardware& rhi, const GeometryRenderData& data) override;

        bool AcquireResources(const UUID& id);
        void ReleaseResources(const UUID& id);

    private:
        struct VulkanDescriptorState
        {
            uint32_t Generation[3];
            UUID Id[3];
        };

        struct VulkanMaterialInstanceLocalState
        {
            size_t m_InstanceIndex;
            VkDescriptorSet m_DescriptorSet[3];
            std::vector<VulkanDescriptorState> m_DescriptorStates;
        };

        VkAllocationCallbacks* m_Allocator;
        VkDevice m_Device;

        std::vector<VulkanShader> m_Shaders;

        std::unique_ptr<VulkanPipeline> m_Pipeline;

        VkDescriptorPool m_GlobalDescriptorPool;
        VkDescriptorSetLayout m_GlobalDescriptorSetLayout;
        VkDescriptorSet* m_GlobalDescriptorSets;

        std::unique_ptr<VulkanBuffer> m_GlobalUniformBuffer;

        std::vector<VkDescriptorType> m_LocalDescriptorTypes;
        VkDescriptorPool m_LocalDescriptorPool;
        VkDescriptorSetLayout m_LocalDescriptorSetLayout;
        std::unordered_map<UUID, VulkanMaterialInstanceLocalState> m_InstanceLocalStates;

        std::unique_ptr<VulkanBuffer> m_LocalUniformBuffer;
        uint32_t m_LocalUniformBufferIndex;
    };
} // VoidArchitect
