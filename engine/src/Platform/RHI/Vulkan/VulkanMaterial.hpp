//
// Created by Michael Desmedt on 20/05/2025.
//
#pragma once

#include "Resources/Material.hpp"
#include "Resources/RenderState.hpp"

#include <vulkan/vulkan.h>

namespace VoidArchitect::Platform
{
    class VulkanRHI;
    class VulkanShader;
    class VulkanDevice;
    class VulkanSwapchain;
    class VulkanRenderPass;
    class VulkanPipeline;
    class VulkanBuffer;

    class VulkanMaterial : public Resources::IMaterial
    {
    public:
        VulkanMaterial(
            const std::string& name,
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator);
        ~VulkanMaterial() override;

        void InitializeResources(
            IRenderingHardware& rhi,
            const VAArray<Renderer::ResourceBinding>& bindings) override;

        void Bind(IRenderingHardware& rhi, const Resources::RenderStatePtr& pipeline) override;
        void SetModel(
            IRenderingHardware& rhi,
            const Math::Mat4& model,
            const Resources::RenderStatePtr& pipeline) override;

    private:
        void ReleaseResources() override;
        void UpdateDescriptorSets(VulkanRHI& rhi);

        // Material description
        VAArray<Renderer::ResourceBinding> m_PipelineResourceBindings;

        VkAllocationCallbacks* m_Allocator;
        VkDevice m_Device;

        uint32_t m_BufferSlot = std::numeric_limits<uint32_t>::max();

        VkDescriptorPool m_MaterialDescriptorPool;
        VkDescriptorSet m_MaterialDescriptorSets[3];

        struct DescriptorState
        {
            uint32_t matGeneration = std::numeric_limits<uint32_t>::max();
            uint32_t texGeneration = std::numeric_limits<uint32_t>::max();

            std::vector<u_int32_t> resourcesGenerations;
            std::vector<UUID> resourcesUUIDs;
        };

        std::array<DescriptorState, 3> m_MaterialDescriptorStates;
    };
} // namespace VoidArchitect::Platform
