//
// Created by Michael Desmedt on 08/06/2025.
//
#include "VulkanBindingGroupManager.hpp"

#include <ranges>

#include "VulkanBuffer.hpp"
#include "VulkanDevice.hpp"
#include "VulkanUtils.hpp"
#include "Systems/MaterialSystem.hpp"

namespace VoidArchitect::Platform
{
    VulkanBindingGroupManager::VulkanBindingGroupManager(
        std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator)
        : m_Device(device),
          m_Allocator(allocator)
    {
        VAArray<VkDescriptorPoolSize> poolSizes = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_MATERIALS},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_MATERIALS * 4}
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = MAX_MATERIALS;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkCreateDescriptorPool(m_Device->GetLogicalDeviceHandle(), &poolInfo, m_Allocator, &
                m_DescriptorPool))

        const VkDeviceSize minAlignment = m_Device->GetProperties().limits.
                                                    minUniformBufferOffsetAlignment;
        m_MaterialUniformBufferSize = (sizeof(Resources::MaterialUniformObject) + minAlignment - 1)
            & ~(minAlignment - 1);
        const size_t uboSize = m_MaterialUniformBufferSize * MAX_MATERIALS;
        m_MaterialUniformBuffer = std::make_unique<VulkanBuffer>(
            m_Device,
            m_Allocator,
            uboSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        m_MaterialUniformBufferMemory = m_MaterialUniformBuffer->LockMemory(0, uboSize, 0);
        VA_ENGINE_INFO(
            "[VulkanBindingGroupManager] Material UBO created for {} materials, slot size {} bytes, total size: {} bytes.",
            MAX_MATERIALS,
            m_MaterialUniformBufferSize,
            uboSize);
    }

    VulkanBindingGroupManager::~VulkanBindingGroupManager()
    {
        m_MaterialUniformBuffer->UnlockMemory();
        vkDestroyDescriptorPool(m_Device->GetLogicalDeviceHandle(), m_DescriptorPool, m_Allocator);
        for (const auto& val : m_SetLayoutCache | std::views::values)
        {
            vkDestroyDescriptorSetLayout(m_Device->GetLogicalDeviceHandle(), val, m_Allocator);
        }
    }

    void VulkanBindingGroupManager::BindMaterialGroup(
        VkCommandBuffer cmdsBuf,
        VkPipelineLayout pipelineLayout,
        MaterialHandle materialHandle,
        RenderStateHandle stateHandle)
    {
        // 1. Check compatibility
        if (!AreLayoutCompatible(materialHandle, stateHandle))
        {
            //TODO: Use a default error material.
            return;
        }

        // 2. Obtain the descriptor set for this material
        auto it = m_MaterialSetCache.find(materialHandle);
        VkDescriptorSet materialSet;
        if (it == m_MaterialSetCache.end())
        {
            auto layout = GetHandleFor(materialHandle);
            if (layout == VK_NULL_HANDLE)
            {
                VA_ENGINE_ERROR("[VulkanBindingGroupManager] Failed to get layout.");
                return;
            }
            materialSet = AllocateSet(layout);
            UpdateDescriptorSet(materialSet, materialHandle);
            m_MaterialSetCache[materialHandle] = materialSet;
        }

        else
        {
            materialSet = it->second;
            //TODO: Add dirty flag logic
            // if(g_MaterialSystem->IsDirty(materialHandle)) {
            //  UpdateDescriptorSet(materialSet, materialHandle);
            // }
        }

        // 3. Bind the descriptor set
        vkCmdBindDescriptorSets(
            cmdsBuf,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            1,
            1,
            &materialSet,
            0,
            nullptr);
    }

    void VulkanBindingGroupManager::UpdateMaterialUBO(
        const MaterialHandle materialHandle,
        const Resources::MaterialUniformObject& ubo)
    {
        auto it = m_MaterialUboOffsets.find(materialHandle);
        if (it == m_MaterialUboOffsets.end())
        {
            size_t offset = m_NextFreeUboOffset;
            m_NextFreeUboOffset += m_MaterialUniformBufferSize;
            // TODO: Check it doesn't go out of bounds.
            m_MaterialUboOffsets[materialHandle] = offset;
            it = m_MaterialUboOffsets.find(materialHandle);
        }

        uint8_t* dest = static_cast<uint8_t*>(m_MaterialUniformBufferMemory) + it->second;
        memcpy(dest, &ubo, sizeof(Resources::MaterialUniformObject));
    }

    bool VulkanBindingGroupManager::AreLayoutCompatible(
        MaterialHandle MaterialHandle,
        RenderStateHandle stateHandle)
    {
        // const auto& materialConfig = g_MaterialSystem->GetTemplateFor(MaterialHandle);
        // const auto& stateConfig = g_RenderStateSystem->GetConfigFor(stateHandle);
        //
        // if (materialConfig.resourceBindings.size() != stateConfig.resourceBindings.size())
        // {
        //     VA_ENGINE_ERROR("[VulkanBindingGroupManager] Incompatible layout.");
        //     return false;
        // }

        //TODO: We should compare every member.

        return true;
    }

    VkDescriptorSetLayout VulkanBindingGroupManager::GetHandleFor(MaterialHandle materialHandle)
    {
        const auto& materialConfig = g_MaterialSystem->GetTemplateFor(materialHandle);
        const size_t hash = materialConfig.GetHash();

        auto it = m_SetLayoutCache.find(hash);
        if (it != m_SetLayoutCache.end())
        {
            return it->second;
        }

        VAArray<VkDescriptorSetLayoutBinding> vkBindings;
        for (const auto& bindingConfig : materialConfig.resourceBindings)
        {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = bindingConfig.binding;
            binding.descriptorType = TranslateEngineResourceTypeToVulkan(bindingConfig.type);
            binding.descriptorCount = 1;
            binding.stageFlags = TranslateEngineShaderStageToVulkan(bindingConfig.stage);
            vkBindings.push_back(binding);
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
        layoutInfo.pBindings = vkBindings.data();

        VkDescriptorSetLayout layout;
        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkCreateDescriptorSetLayout(
                m_Device->GetLogicalDeviceHandle(), &layoutInfo, m_Allocator, &layout));

        m_SetLayoutCache[hash] = layout;
        return layout;
    }

    VkDescriptorSet VulkanBindingGroupManager::AllocateSet(VkDescriptorSetLayout layout)
    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_DescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        VkDescriptorSet set;
        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkAllocateDescriptorSets(m_Device->GetLogicalDeviceHandle(), &allocInfo, &set));
        return set;
    }

    void VulkanBindingGroupManager::UpdateDescriptorSet(
        VkDescriptorSet set,
        MaterialHandle materialHandle)
    {
        const auto& materialConfig = g_MaterialSystem->GetTemplateFor(materialHandle);

        // Pointer storage must live until the end
        VAArray<VkWriteDescriptorSet> descriptorWrites;
        VkDescriptorBufferInfo bufferInfo;
        VAArray<VkDescriptorImageInfo> imageInfos;
        imageInfos.reserve(2); // Diffuse and Specular for now

        for (const auto& bindingConfig : materialConfig.resourceBindings)
        {
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = set;
            write.dstBinding = bindingConfig.binding;
            write.dstArrayElement = 0;
            write.descriptorCount = 1;

            if (bindingConfig.type == Renderer::ResourceBindingType::ConstantBuffer)
            {
                write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

                bufferInfo.buffer = m_MaterialUniformBuffer->GetHandle();
                bufferInfo.offset = m_MaterialUboOffsets.at(materialHandle);
                bufferInfo.range = sizeof(Resources::MaterialUniformObject);

                write.pBufferInfo = &bufferInfo;
            }
            else if (bindingConfig.type == Renderer::ResourceBindingType::Texture2D)
            {
                write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                //TODO: Retrieve texture from the TextureSystem
            }

            descriptorWrites.push_back(write);
        }

        vkUpdateDescriptorSets(
            m_Device->GetLogicalDeviceHandle(),
            static_cast<uint32_t>(descriptorWrites.size()),
            descriptorWrites.data(),
            0,
            nullptr);
    }
}
