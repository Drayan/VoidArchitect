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
        VulkanMaterial(
            const std::string& name,
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            Resources::PipelinePtr pipeline);
        ~VulkanMaterial() override;

        void InitializeResources(IRenderingHardware& rhi) override;

        void Bind(IRenderingHardware& rhi) override;
        void SetModel(IRenderingHardware& rhi, const Math::Mat4& model) override;

    private:
        void ReleaseResources() override;
        void UpdateDescriptorSets(VulkanRHI& rhi);

        VkAllocationCallbacks* m_Allocator;
        VkDevice m_Device;
        Resources::PipelinePtr m_Pipeline;

        uint32_t m_BufferSlot = std::numeric_limits<uint32_t>::max();

        VkDescriptorPool m_MaterialDescriptorPool;
        VkDescriptorSet m_MaterialDescriptorSets[3];

        struct DescriptorState
        {
            uint32_t matGeneration = std::numeric_limits<uint32_t>::max();
            uint32_t texGeneration = std::numeric_limits<uint32_t>::max();
            UUID texUUID = InvalidUUID;
        };

        std::array<DescriptorState, 3> m_MaterialDescriptorStates;
    };
} // namespace VoidArchitect::Platform
