//
// Created by Michael Desmedt on 18/05/2025.
//
#pragma once
#include <vulkan/vulkan.h>

#include "Resources/Pipeline.hpp"

#include "Systems/PipelineSystem.hpp"
#include "VulkanCommandBuffer.hpp"

namespace VoidArchitect
{
    struct PipelineConfig;
}

namespace VoidArchitect::Platform
{
    class VulkanDevice;
    class VulkanRenderpass;
    class VulkanCommandBuffer;
    class VulkanShader;

    class VulkanPipeline : public Resources::IPipeline
    {
    public:
        VulkanPipeline(
            const PipelineConfig& config,
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            const std::unique_ptr<VulkanRenderpass>& renderPass);
        ~VulkanPipeline() override;

        // void Bind(const VulkanCommandBuffer& cmdBuf, VkPipelineBindPoint bindPoint) const;
        void Bind(Platform::IRenderingHardware& rhi) override;
        [[nodiscard]] VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }
        VkPipeline GetHandle() const { return m_Pipeline; }

        VkDescriptorSetLayout GetDescriptorSetLayout(const size_t index) const
        {
            return m_DescriptorSetLayouts[index];
        }

        VkDescriptorSetLayout GetGlobalDescriptorSetLayout() const
        {
            return m_DescriptorSetLayouts[0];
        }

        VkDescriptorSetLayout GetMaterialDescriptorSetLayout() const
        {
            return m_DescriptorSetLayouts[1];
        }

    private:
        static VkFormat TranslateEngineAttributeFormatToVulkanFormat(
            VertexAttributeType type, AttributeFormat format);
        static uint32_t GetEngineAttributeSize(VertexAttributeType type, AttributeFormat format);

        VkDevice m_Device;
        VkAllocationCallbacks* m_Allocator;

        std::vector<Resources::ShaderPtr> m_Shaders;

        VkPipeline m_Pipeline;

        VkPipelineLayout m_PipelineLayout;
        std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;
    };
} // namespace VoidArchitect::Platform
