//
// Created by Michael Desmedt on 20/05/2025.
//
#pragma once

#include "Resources/Material.hpp"
#include "Resources/Pipeline.hpp"

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

    class VulkanMaterial : public Resources::IMaterial
    {
    public:
        constexpr static size_t MAX_INSTANCES = 1024;

        VulkanMaterial(
            const std::string& name,
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            const Resources::PipelinePtr& pipeline);
        ~VulkanMaterial() override;

        void InitializeResources(IRenderingHardware& rhi) override;

        void SetObject(IRenderingHardware& rhi, const Resources::GeometryRenderData& data) override;

        bool AcquireResources(const UUID& id);
        void ReleaseResources(const UUID& id);

    private:
        void ReleaseResources() override;

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

        Resources::PipelinePtr m_Pipeline;

        std::vector<VkDescriptorType> m_LocalDescriptorTypes;
        VkDescriptorPool m_LocalDescriptorPool;
        std::unordered_map<UUID, VulkanMaterialInstanceLocalState> m_InstanceLocalStates;

        std::unique_ptr<VulkanBuffer> m_LocalUniformBuffer;
        uint32_t m_LocalUniformBufferIndex;
    };
} // namespace VoidArchitect::Platform
