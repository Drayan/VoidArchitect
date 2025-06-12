//
// Created by Michael Desmedt on 18/05/2025.
//
#pragma once
#include <vulkan/vulkan.h>

#include "Resources/RenderState.hpp"

#include "Systems/RenderStateSystem.hpp"
#include "VulkanCommandBuffer.hpp"

namespace VoidArchitect
{
    struct RenderStateConfig;
}

namespace VoidArchitect::Platform
{
    class VulkanDevice;
    class VulkanRenderPass;
    class VulkanCommandBuffer;
    class VulkanShader;

    class VulkanPipeline : public Resources::IRenderState
    {
    public:
        VulkanPipeline(
            const std::string& name,
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            VkPipeline pipelineHandle,
            VkPipelineLayout layoutHandle);
        ~VulkanPipeline() override;

        // void Bind(const VulkanCommandBuffer& cmdBuf, VkPipelineBindPoint bindPoint) const;
        void Bind(Platform::IRenderingHardware& rhi) override;
        [[nodiscard]] VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }
        VkPipeline GetHandle() const { return m_Pipeline; }

    private:
        static VkFormat TranslateEngineAttributeFormatToVulkanFormat(
            Renderer::AttributeType type,
            Renderer::AttributeFormat format);

        static uint32_t GetEngineAttributeSize(
            Renderer::AttributeType type,
            Renderer::AttributeFormat format);

        VkDevice m_Device;
        VkAllocationCallbacks* m_Allocator;

        VkPipeline m_Pipeline;
        VkPipelineLayout m_PipelineLayout;
    };
} // namespace VoidArchitect::Platform
