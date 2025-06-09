//
// Created by Michael Desmedt on 20/05/2025.
//
#include "VulkanMaterial.hpp"

#include <ranges>
#include <utility>

#include "VulkanBuffer.hpp"
#include "VulkanCommandBuffer.hpp"
#include "VulkanDescriptorSetLayoutManager.hpp"
#include "VulkanExecutionContext.hpp"
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

        for (auto& descriptorState : m_MaterialDescriptorStates)
            descriptorState = DescriptorState{};
    }

    VulkanMaterial::~VulkanMaterial() { VulkanMaterial::ReleaseResources(); }

    void VulkanMaterial::InitializeResources(
        IRenderingHardware& rhi,
        const VAArray<Renderer::ResourceBinding>& bindings)
    {
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
        m_PipelineResourceBindings = bindings;

        VAArray<VkDescriptorPoolSize> poolSizes;
        poolSizes.reserve(bindings.size());

        for (const auto& binding : m_PipelineResourceBindings)
        {
            for (auto& descriptorState : m_MaterialDescriptorStates)
            {
                // Insert an invalid generation for each resource at their binding location.
                descriptorState.resourcesGenerations.push_back(
                    std::numeric_limits<uint32_t>::max());
                descriptorState.resourcesUUIDs.push_back(InvalidUUID);
            }

            switch (binding.type)
            {
                case Renderer::ResourceBindingType::ConstantBuffer:
                    poolSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3});
                    break;
                case Renderer::ResourceBindingType::Texture1D:
                case Renderer::ResourceBindingType::Texture2D:
                case Renderer::ResourceBindingType::Texture3D:
                    poolSizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3});
                    break;
                default:
                    break;
            }
        }

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
        const auto& cmdBuf = vulkanRhi.GetExecutionContextRef()->GetCurrentCommandBuffer();
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
        const uint32_t frameIndex = vulkanRhi.GetExecutionContextRef()->GetImageIndex();
        const auto& descriptorState = m_MaterialDescriptorStates[frameIndex];

        // 1. Check if we need to update the material descriptor set.
        // TODO: This should be changed and split into two functions.
        if (descriptorState.resourcesGenerations[0] != m_Generation)
        {
            const auto materialBO = Resources::MaterialUniformObject{
                .DiffuseColor = m_DiffuseColor,
            };
            g_VkMaterialBufferManager->UpdateMaterial(m_BufferSlot, materialBO);
        }

        UpdateDescriptorSets(vulkanRhi);

        const auto& cmdBuf = vulkanRhi.GetExecutionContextRef()->GetCurrentCommandBuffer();
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
        const uint32_t frameIndex = rhi.GetExecutionContextRef()->GetImageIndex();
        auto& descriptorState = m_MaterialDescriptorStates[frameIndex];

        VAArray<VkDescriptorBufferInfo> bufferInfos{};
        VAArray<VkDescriptorImageInfo> imageInfos{};
        uint32_t bufferInfoIndex = 0;
        uint32_t imageInfoIndex = 0;
        VAArray<VkWriteDescriptorSet> writes{};

        bufferInfos.reserve(m_PipelineResourceBindings.size());
        imageInfos.reserve(m_PipelineResourceBindings.size());
        writes.reserve(m_PipelineResourceBindings.size());

        // 1. If we need, we update the material descriptor set for uniform buffer.
        if (descriptorState.matGeneration != m_Generation)
        {
            for (const auto& resBinding : m_PipelineResourceBindings)
            {
                switch (resBinding.type)
                {
                    case Renderer::ResourceBindingType::ConstantBuffer:
                    {
                        // If the resource generation is different from the one we have in the
                        // descriptor, it means we have to update it.
                        if (descriptorState.resourcesGenerations[resBinding.binding]
                            == m_Generation)
                            continue;

                        VkDescriptorBufferInfo bufferInfo{};
                        auto [buffer, offset, range] =
                            g_VkMaterialBufferManager->GetBindingInfo(m_BufferSlot);

                        bufferInfo.buffer = buffer;
                        bufferInfo.offset = offset;
                        bufferInfo.range = range;

                        // We store this because we need a valid pointer for Vulkan.
                        bufferInfos.push_back(bufferInfo);

                        VkWriteDescriptorSet write{};
                        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        write.dstSet = m_MaterialDescriptorSets[frameIndex];
                        write.dstBinding = resBinding.binding;
                        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                        write.descriptorCount = 1;
                        write.pBufferInfo = &bufferInfos[bufferInfoIndex++];

                        writes.push_back(write);

                        descriptorState.resourcesGenerations[resBinding.binding] = m_Generation;
                    }
                    break;

                    case Renderer::ResourceBindingType::Texture1D:
                    case Renderer::ResourceBindingType::Texture2D:
                    case Renderer::ResourceBindingType::Texture3D:
                    {
                        // Retrieve the texture.
                        auto texture =
                            std::dynamic_pointer_cast<VulkanTexture2D>(m_Textures[imageInfoIndex]);
                        // If we cannot find the texture, or if the texture generation is invalid,
                        // we use the default texture.
                        if (!texture
                            || texture->GetGeneration() == std::numeric_limits<uint32_t>::max())
                        {
                            texture =
                                std::dynamic_pointer_cast<VulkanTexture2D>(s_DefaultDiffuseTexture);
                        }

                        // If we can't have a texture by this point, or if the texture hasn't
                        // changed, we skip this resource.
                        if (!texture
                            || (descriptorState.resourcesUUIDs[resBinding.binding]
                                == texture->GetUUID()
                                && descriptorState.resourcesGenerations[resBinding.binding]
                                == texture->GetGeneration()))
                            continue;

                        VkDescriptorImageInfo imageInfo{};
                        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        imageInfo.imageView = texture->GetImageView();
                        imageInfo.sampler = texture->GetSampler();

                        imageInfos.push_back(imageInfo);

                        VkWriteDescriptorSet write{};
                        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        write.dstSet = m_MaterialDescriptorSets[frameIndex];
                        write.dstBinding = resBinding.binding;
                        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                        write.descriptorCount = 1;
                        write.pImageInfo = &imageInfos[imageInfoIndex++];

                        VA_ENGINE_DEBUG(
                            "[VulkanMaterial] Updating descriptor set for binding {} with texture "
                            ": {}",
                            resBinding.binding,
                            static_cast<uint64_t>(texture->GetUUID()));

                        writes.push_back(write);

                        descriptorState.resourcesUUIDs[resBinding.binding] = texture->GetUUID();
                        descriptorState.resourcesGenerations[resBinding.binding] =
                            texture->GetGeneration();
                    }
                    break;

                    default:
                        break;
                }
            }

            // Update the material generation.
            descriptorState.matGeneration = m_Generation;
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
