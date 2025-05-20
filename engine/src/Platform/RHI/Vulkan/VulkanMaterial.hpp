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

        void SetObjectModelConstant(
            IRenderingHardware& rhi,
            const Math::Mat4& model) override;

    private:
        VkAllocationCallbacks* m_Allocator;
        VkDevice m_Device;

        std::vector<VulkanShader> m_Shaders;

        std::unique_ptr<VulkanPipeline> m_Pipeline;

        VkDescriptorPool m_DescriptorPool;
        VkDescriptorSet* m_DescriptorSets;
        VkDescriptorSetLayout m_DescriptorSetLayout;

        std::unique_ptr<VulkanBuffer> m_GlobalUniformBuffer;
    };
} // VoidArchitect
