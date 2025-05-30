//
// Created by Michael Desmedt on 20/05/2025.
//
#include "VulkanMaterial.hpp"

#include <ranges>

#include "Core/Math/Vec3.hpp"

#include "VulkanBuffer.hpp"
#include "VulkanCommandBuffer.hpp"
#include "VulkanDescriptorSetLayoutManager.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanShader.hpp"
#include "VulkanSwapchain.hpp"
#include "VulkanTexture.hpp"
#include "VulkanUtils.hpp"

namespace VoidArchitect::Platform
{
    constexpr std::string BUILTIN_OBJECT_SHADER_NAME = "BuiltinObject";

    VulkanMaterial::VulkanMaterial(
        const std::string& name,
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        const Resources::PipelinePtr& pipeline)
        : IMaterial(name),
          m_Allocator(allocator),
          m_Device(device->GetLogicalDeviceHandle()),
          m_Pipeline(pipeline),
          m_LocalDescriptorPool{},
          m_LocalUniformBufferIndex{}
    {
    }

    VulkanMaterial::~VulkanMaterial() { VulkanMaterial::ReleaseResources(); }

    void VulkanMaterial::InitializeResources(IRenderingHardware& rhi)
    {
        auto& vulkanRhi = dynamic_cast<VulkanRHI&>(rhi);
        auto& device = vulkanRhi.GetDeviceRef();

        // --- Local Descriptors ---
        // TODO Maybe we should retrieve the pipeline configuration and use that to determine the
        //  resources bindings.
        m_LocalDescriptorTypes = std::vector{
            // Binding 0 - Uniform buffer
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            // Binding 1 - Diffuse sampler layout.
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        };

        std::vector<VkDescriptorPoolSize> localPoolSizes;
        for (auto type : m_LocalDescriptorTypes)
        {
            auto poolSize = VkDescriptorPoolSize{};
            poolSize.type = type;
            poolSize.descriptorCount = MAX_INSTANCES;

            localPoolSizes.push_back(poolSize);
        }

        auto localPoolInfo = VkDescriptorPoolCreateInfo{};
        localPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        localPoolInfo.maxSets = MAX_INSTANCES;
        localPoolInfo.poolSizeCount = localPoolSizes.size();
        localPoolInfo.pPoolSizes = localPoolSizes.data();

        VA_VULKAN_CHECK_RESULT_WARN(
            vkCreateDescriptorPool(m_Device, &localPoolInfo, m_Allocator, &m_LocalDescriptorPool));

        m_LocalUniformBuffer = std::make_unique<VulkanBuffer>(
            vulkanRhi,
            device,
            m_Allocator,
            sizeof(Resources::LocalUniformObject),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VA_ENGINE_INFO("[VulkanMaterial] Material created.");
    }

    void VulkanMaterial::SetObject(
        IRenderingHardware& rhi, const Resources::GeometryRenderData& data)
    {
        auto& vulkanRhi = dynamic_cast<VulkanRHI&>(rhi);
        const auto& cmdBuf = vulkanRhi.GetCurrentCommandBuffer();
        const auto& vkPipeline = std::dynamic_pointer_cast<VulkanPipeline>(m_Pipeline);

        // NOTE VulkanSpec guarantee 128 bytes.
        vkCmdPushConstants(
            cmdBuf.GetHandle(),
            vkPipeline->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(Math::Mat4),
            // 64 bytes
            &data.Model);

        // Obtain material data
        // TEMP
        AcquireResources(data.Material->GetUUID());

        if (m_InstanceLocalStates.contains(data.Material->GetUUID()))
        {
            auto& instanceState = m_InstanceLocalStates[data.Material->GetUUID()];
            auto& localDescriptorSet = instanceState.m_DescriptorSet[vulkanRhi.GetImageIndex()];

            std::vector<VkWriteDescriptorSet> descriptorWrites;

            // Descriptor 0 - Uniform buffer
            auto range = sizeof(Resources::LocalUniformObject);
            auto offset = sizeof(Resources::LocalUniformObject) * instanceState.m_InstanceIndex;

            auto obo = std::vector{Resources::LocalUniformObject{
                .DiffuseColor = Math::Vec4::One(),
            }};

            m_LocalUniformBuffer->LoadData(obo);

            // Only do this if the descriptor has not yet been updated.
            if (instanceState.m_DescriptorStates[0].Generation[vulkanRhi.GetImageIndex()]
                == std::numeric_limits<uint32_t>::max())
            {
                // Update the descriptor set
                VkDescriptorBufferInfo bufferInfo{};
                bufferInfo.buffer = m_LocalUniformBuffer->GetHandle();
                bufferInfo.offset = offset;
                bufferInfo.range = range;

                VkWriteDescriptorSet writeDescriptorSet{};
                writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeDescriptorSet.dstSet = localDescriptorSet;
                writeDescriptorSet.dstBinding = 0; // Local index
                writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                writeDescriptorSet.descriptorCount = 1;
                writeDescriptorSet.pBufferInfo = &bufferInfo;

                descriptorWrites.push_back(writeDescriptorSet);

                // Update the frame generation. In this case it is only necessary once since this is
                // a buffer.
                instanceState.m_DescriptorStates[0].Generation[vulkanRhi.GetImageIndex()] = 1;
            }

            // TODO: Samplers
            const uint32_t samplerCount = 1;
            VkDescriptorImageInfo imageInfos[samplerCount];
            for (uint32_t i = 0; i < samplerCount; i++)
            {
                auto texture =
                    std::dynamic_pointer_cast<VulkanTexture2D>(data.Material->GetTexture(i));
                auto& descriptorGeneration =
                    instanceState.m_DescriptorStates[1].Generation[vulkanRhi.GetImageIndex()];
                auto& descriptorTextureUUID =
                    instanceState.m_DescriptorStates[1].Id[vulkanRhi.GetImageIndex()];

                // If the texture hasn't been loaded yet, use the default.
                // TODO: Determine which use the texture has and pull appropriate default.
                if (texture == nullptr
                    || texture->GetGeneration() == std::numeric_limits<uint32_t>::max())
                {
                    texture = std::dynamic_pointer_cast<VulkanTexture2D>(s_DefaultDiffuseTexture);
                    descriptorGeneration = std::numeric_limits<uint32_t>::max();
                }

                // Check if the descriptor needs updating first.
                if (texture != nullptr
                    && (descriptorTextureUUID != texture->GetUUID()
                        || descriptorGeneration != texture->GetGeneration()
                        || descriptorGeneration == std::numeric_limits<uint32_t>::max()))
                {
                    // Update the descriptor set
                    imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    imageInfos[i].imageView = texture->GetImageView();
                    imageInfos[i].sampler = texture->GetSampler();

                    VkWriteDescriptorSet writeDescriptorSet{};
                    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    writeDescriptorSet.dstSet = localDescriptorSet;
                    writeDescriptorSet.dstBinding = 1; // Local index
                    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    writeDescriptorSet.descriptorCount = 1;
                    writeDescriptorSet.pImageInfo = &imageInfos[i];

                    descriptorWrites.push_back(writeDescriptorSet);

                    if (texture->GetGeneration() != std::numeric_limits<uint32_t>::max())
                    {
                        descriptorGeneration = texture->GetGeneration();
                        descriptorTextureUUID = texture->GetUUID();
                    }
                }
            }

            if (!descriptorWrites.empty())
            {
                vkUpdateDescriptorSets(
                    m_Device,
                    static_cast<uint32_t>(descriptorWrites.size()),
                    descriptorWrites.data(),
                    0,
                    nullptr);
            }

            // Bind the descriptor set to be updated, or in case the shader(material) changed.
            vkCmdBindDescriptorSets(
                cmdBuf.GetHandle(),
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                vkPipeline->GetPipelineLayout(),
                1,
                1,
                &localDescriptorSet,
                0,
                nullptr);
        }
    }

    bool VulkanMaterial::AcquireResources(const UUID& id)
    {
        // If this object is not listed into the instances, add it.
        if (!m_InstanceLocalStates.contains(id))
        {
            auto instanceState = VulkanMaterialInstanceLocalState{};
            instanceState.m_InstanceIndex = m_LocalUniformBufferIndex++;
            for (uint32_t i = 0; i < m_LocalDescriptorTypes.size(); i++)
            {
                const VulkanDescriptorState state{
                    .Generation =
                        {std::numeric_limits<uint32_t>::max(),
                         std::numeric_limits<uint32_t>::max(),
                         std::numeric_limits<uint32_t>::max()},
                    .Id = {InvalidUUID, InvalidUUID, InvalidUUID}};
                instanceState.m_DescriptorStates = std::vector{state, state, state};
            }

            // Allocate descriptor sets
            const auto materialDescriptorSetLayout =
                g_VkDescriptorSetLayoutManager->GetPerMaterialLayout();
            const VkDescriptorSetLayout layouts[] = {
                materialDescriptorSetLayout,
                materialDescriptorSetLayout,
                materialDescriptorSetLayout};

            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = m_LocalDescriptorPool;
            allocInfo.descriptorSetCount = 3;
            allocInfo.pSetLayouts = layouts;

            if (VA_VULKAN_CHECK_RESULT(
                    vkAllocateDescriptorSets(m_Device, &allocInfo, instanceState.m_DescriptorSet)))
            {
                VA_ENGINE_ERROR("[VulkanMaterial] Failed to allocate descriptor sets.");
                return false;
            }

            m_InstanceLocalStates[id] = instanceState;

            return true;
        }

        return false;
    }

    void VulkanMaterial::ReleaseResources(const UUID& id)
    {
        if (m_InstanceLocalStates.contains(id))
        {
            m_InstanceLocalStates.erase(id);
        }
    }
    void VulkanMaterial::ReleaseResources()
    {
        // We should release any instances remaining.
        m_InstanceLocalStates.clear();

        if (m_LocalUniformBuffer != nullptr)
        {
            m_LocalUniformBuffer = nullptr;
            VA_ENGINE_DEBUG("[VulkanMaterial] Local uniform buffer destroyed.");
        }

        if (m_Pipeline != nullptr)
        {
            m_Pipeline = nullptr;
        }

        if (m_LocalDescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_Device, m_LocalDescriptorPool, m_Allocator);
            m_LocalDescriptorPool = VK_NULL_HANDLE;
            VA_ENGINE_DEBUG("[VulkanMaterial] Descriptor pool destroyed.");
        }
    }
} // namespace VoidArchitect::Platform
