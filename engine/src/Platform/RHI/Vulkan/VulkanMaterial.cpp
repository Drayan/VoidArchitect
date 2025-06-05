//
// Created by Michael Desmedt on 20/05/2025.
//
#include "VulkanMaterial.hpp"

#include <ranges>
#include <utility>

#include "VulkanBuffer.hpp"
#include "VulkanCommandBuffer.hpp"
#include "VulkanDescriptorSetLayoutManager.hpp"
#include "VulkanMaterialBufferManager.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanSwapchain.hpp"
#include "VulkanTexture.hpp"
#include "VulkanUtils.hpp"

namespace VoidArchitect::Platform
{
    VulkanMaterial::VulkanMaterial(
        const std::string& name,
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator)
        : IMaterial(name),
          m_Allocator(allocator),
          m_Device(device->GetLogicalDeviceHandle()),
          m_MaterialDescriptorPool(VK_NULL_HANDLE),
          m_MaterialDescriptorSets{}
    {
        for (auto& descriptorSet : m_MaterialDescriptorSets)
            descriptorSet = VK_NULL_HANDLE;
    }

    VulkanMaterial::~VulkanMaterial() { VulkanMaterial::ReleaseResources(); }

    void VulkanMaterial::InitializeResources(IRenderingHardware& rhi)
    {
        auto& vulkanRhi = dynamic_cast<VulkanRHI&>(rhi);
        auto& device = vulkanRhi.GetDeviceRef();

        if (!g_VkMaterialBufferManager)
        {
            VA_ENGINE_CRITICAL("[VulkanMaterial] Material buffer manager not initialized.");
            return;
        }

        m_BufferSlot = g_VkMaterialBufferManager->AllocateSlot(m_UUID);

        if (m_BufferSlot == std::numeric_limits<uint32_t>::max())
        {
            VA_ENGINE_ERROR("[VulkanMaterial] Failed to allocate material buffer slot.");
            return;
        }

        // --- Local Descriptors ---
        // TODO Maybe we should retrieve the pipeline configuration and use that to determine the
        //  resources bindings.
        std::vector<VkDescriptorPoolSize> poolSizes = {
            // Binding 0 - Uniform buffer
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
            // Binding 1 - Diffuse sampler layout.
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3}
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = 3;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        VA_VULKAN_CHECK_RESULT_WARN(
            vkCreateDescriptorPool(m_Device, &poolInfo, m_Allocator, &m_MaterialDescriptorPool));

        const auto materialLayout = g_VkDescriptorSetLayoutManager->GetPerMaterialLayout();
        const VkDescriptorSetLayout layouts[] = {materialLayout, materialLayout, materialLayout};

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_MaterialDescriptorPool;
        allocInfo.descriptorSetCount = 3;
        allocInfo.pSetLayouts = layouts;

        VA_VULKAN_CHECK_RESULT_WARN(
            vkAllocateDescriptorSets(m_Device, &allocInfo, m_MaterialDescriptorSets));

        VA_ENGINE_INFO("[VulkanMaterial] Material '{}' created.", m_Name);
    }

    void VulkanMaterial::SetModel(
        IRenderingHardware& rhi,
        const Math::Mat4& model,
        const Resources::RenderStatePtr& pipeline)
    {
        auto& vulkanRhi = dynamic_cast<VulkanRHI&>(rhi);
        const auto& cmdBuf = vulkanRhi.GetCurrentCommandBuffer();
        const auto& vkPipeline = std::dynamic_pointer_cast<VulkanPipeline>(pipeline);

        vkCmdPushConstants(
            cmdBuf.GetHandle(),
            vkPipeline->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(Math::Mat4),
            // 64 bytes
            &model);
    }

    void VulkanMaterial::Bind(IRenderingHardware& rhi, const Resources::RenderStatePtr& pipeline)
    {
        auto& vulkanRhi = dynamic_cast<VulkanRHI&>(rhi);
        const uint32_t frameIndex = vulkanRhi.GetImageIndex();
        const auto& descriptorState = m_MaterialDescriptorStates[frameIndex];

        // 1. Check if we need to update the material descriptor set.
        if (descriptorState.matGeneration != m_Generation)
        {
            const auto materialBO = Resources::MaterialUniformObject{
                .DiffuseColor = m_DiffuseColor,
            };
            g_VkMaterialBufferManager->UpdateMaterial(m_BufferSlot, materialBO);
        }

        UpdateDescriptorSets(vulkanRhi);

        const auto& cmdBuf = vulkanRhi.GetCurrentCommandBuffer();
        const auto& vkPipeline = std::dynamic_pointer_cast<VulkanPipeline>(pipeline);

        // Bind the descriptor set to be updated, or in case the shader(material) changed.
        vkCmdBindDescriptorSets(
            cmdBuf.GetHandle(),
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            vkPipeline->GetPipelineLayout(),
            1,
            1,
            &m_MaterialDescriptorSets[frameIndex],
            0,
            nullptr);
    }

    void VulkanMaterial::ReleaseResources()
    {
        vkFreeDescriptorSets(m_Device, m_MaterialDescriptorPool, 3, m_MaterialDescriptorSets);

        if (m_BufferSlot != std::numeric_limits<uint32_t>::max())
        {
            g_VkMaterialBufferManager->ReleaseSlot(m_BufferSlot);
            m_BufferSlot = std::numeric_limits<uint32_t>::max();
        }

        if (m_MaterialDescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_Device, m_MaterialDescriptorPool, m_Allocator);
            m_MaterialDescriptorPool = VK_NULL_HANDLE;
            VA_ENGINE_DEBUG("[VulkanMaterial] Descriptor pool destroyed.");
        }
    }

    void VulkanMaterial::UpdateDescriptorSets(VulkanRHI& rhi)
    {
        const uint32_t frameIndex = rhi.GetImageIndex();
        auto& descriptorState = m_MaterialDescriptorStates[frameIndex];

        VkDescriptorBufferInfo bufferInfo{};
        std::vector<VkWriteDescriptorSet> writes{};

        // 1. If we need, we update the material descriptor set for uniform buffer.
        if (descriptorState.matGeneration != m_Generation)
        {
            auto bindingInfo = g_VkMaterialBufferManager->GetBindingInfo(m_BufferSlot);

            bufferInfo.buffer = bindingInfo.buffer;
            bufferInfo.offset = bindingInfo.offset;
            bufferInfo.range = bindingInfo.range;

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_MaterialDescriptorSets[frameIndex];
            write.dstBinding = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo = &bufferInfo;

            writes.push_back(write);

            descriptorState.matGeneration = m_Generation;
        }

        // 2. Update textures
        auto texture = std::dynamic_pointer_cast<VulkanTexture2D>(m_DiffuseTexture);

        if (!texture || texture->GetGeneration() == std::numeric_limits<uint32_t>::max())
        {
            texture = std::dynamic_pointer_cast<VulkanTexture2D>(s_DefaultDiffuseTexture);
        }

        VkDescriptorImageInfo imageInfo{};
        if (texture
            && (descriptorState.texUUID != texture->GetUUID()
                || descriptorState.texGeneration != texture->GetGeneration()
                || descriptorState.matGeneration != m_Generation))
        {
            //FIXME: This could be optimized out by the compiler because we only use a pointer to it
            //      therefore this is destroyed as soon as we leave the scope.
            //      We probably should maintain a copy of imageInfo outside the scope.
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = texture->GetImageView();
            imageInfo.sampler = texture->GetSampler();

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_MaterialDescriptorSets[frameIndex];
            write.dstBinding = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 1;
            write.pImageInfo = &imageInfo;

            writes.push_back(write);

            descriptorState.texUUID = texture->GetUUID();
            descriptorState.texGeneration = texture->GetGeneration();
        }

        // 3. Apply updates
        if (!writes.empty())
        {
            vkUpdateDescriptorSets(
                m_Device,
                static_cast<uint32_t>(writes.size()),
                writes.data(),
                0,
                nullptr);
        }
    }
} // namespace VoidArchitect::Platform
