//
// Created by Michael Desmedt on 18/05/2025.
//
#pragma once
#include <vulkan/vulkan.h>

#include "VulkanCommandBuffer.hpp"

namespace VoidArchitect::Platform
{
    class VulkanDevice;
    class VulkanRenderpass;
    class VulkanCommandBuffer;
    class VulkanShader;

    class VulkanPipeline
    {
    public:
        VulkanPipeline(
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            const std::unique_ptr<VulkanRenderpass>& renderPass,
            const std::vector<VulkanShader>& shaders,
            const std::vector<VkVertexInputAttributeDescription>& attributes,
            const std::vector<VkDescriptorSetLayout>& descriptorSets);
        ~VulkanPipeline();

        void Bind(const VulkanCommandBuffer& cmdBuf, VkPipelineBindPoint bindPoint) const;
        [[nodiscard]] VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

    private:
        VkDevice m_Device;
        VkAllocationCallbacks* m_Allocator;

        VkPipelineLayout m_PipelineLayout;
        VkPipeline m_Pipeline;
    };
} // namespace VoidArchitect::Platform
