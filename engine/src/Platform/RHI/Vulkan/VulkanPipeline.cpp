//
// Created by Michael Desmedt on 18/05/2025.
//
#include "VulkanPipeline.hpp"

#include "Core/Math/Mat4.hpp"
#include "VulkanDevice.hpp"
#include "VulkanExecutionContext.hpp"
#include "VulkanRhi.hpp"
#include "VulkanUtils.hpp"

namespace VoidArchitect::Platform
{
    // NOTE The Renderpass is currently completely hard coded, so we will just pass it from the RHI
    //  but it should comes from the config in the future.
    VulkanPipeline::VulkanPipeline(
        const std::string& name,
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        VkPipeline pipelineHandle,
        VkPipelineLayout layoutHandle)
        : IRenderState(name),
          m_Device(device->GetLogicalDeviceHandle()),
          m_Allocator(allocator),
          m_Pipeline(pipelineHandle),
          m_PipelineLayout(layoutHandle)
    {
        // --- Descriptor sets ---
        // m_DescriptorSetLayouts.resize(config.inputLayout.spaces.size());
        //
        // VAArray<VkDescriptorSetLayoutCreateInfo> descriptorSetLayoutsInfos;
        // for (uint32_t i = 0; i < config.inputLayout.spaces.size(); i++)
        // {
        //     const auto& space = config.inputLayout.spaces[i];
        //     VAArray<VkDescriptorSetLayoutBinding> bindings;
        //     for (uint32_t j = 0; j < space.bindings.size(); j++)
        //     {
        //         const auto& [type, binding, stage, buf] = space.bindings[j];
        //
        //         VkDescriptorSetLayoutBinding descriptorSetLayoutBinding{};
        //         descriptorSetLayoutBinding.binding = binding;
        //         descriptorSetLayoutBinding.descriptorType = TranslateEngineResourceTypeToVulkan(
        //             type);
        //         descriptorSetLayoutBinding.descriptorCount = 1;
        //         descriptorSetLayoutBinding.stageFlags = TranslateEngineShaderStageToVulkan(stage);
        //
        //         bindings.push_back(descriptorSetLayoutBinding);
        //     }
        //
        //     auto descriptorSetLayoutCreateInfo = VkDescriptorSetLayoutCreateInfo{};
        //     descriptorSetLayoutCreateInfo.sType =
        //         VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        //     descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        //     descriptorSetLayoutCreateInfo.pBindings = bindings.data();
        //
        //     VA_VULKAN_CHECK_RESULT_WARN(
        //         vkCreateDescriptorSetLayout( m_Device, &descriptorSetLayoutCreateInfo, m_Allocator,
        //             &m_DescriptorSetLayouts[i ]));
        //
        //     VA_ENGINE_TRACE(
        //         "[VulkanPipeline] Descriptor set layout {} created, for pipeline '{}'.",
        //         i,
        //         config.name);
        // }
    }

    VulkanPipeline::~VulkanPipeline()
    {
        if (m_PipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(m_Device, m_PipelineLayout, m_Allocator);
            VA_ENGINE_TRACE("[VulkanPipeline] Pipeline layout destroyed.");
        }
        if (m_Pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(m_Device, m_Pipeline, m_Allocator);
            VA_ENGINE_TRACE("[VulkanPipeline] Pipeline destroyed.");
        }
    }

    void VulkanPipeline::Bind(IRenderingHardware& rhi)
    {
        vkCmdBindPipeline(
            g_VkExecutionContext->GetCurrentCommandBuffer().GetHandle(),
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_Pipeline);
    }
} // namespace VoidArchitect::Platform
