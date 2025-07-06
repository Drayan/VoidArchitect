//
// Created by Michael Desmedt on 08/06/2025.
//
#include "VulkanBindingGroupManager.hpp"

#include <ranges>

#include "VulkanBuffer.hpp"
#include "VulkanDevice.hpp"
#include "VulkanMaterial.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanTexture.hpp"
#include "VulkanUtils.hpp"
#include "Systems/MaterialSystem.hpp"
#include "Systems/TextureSystem.hpp"

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

    VkDescriptorSetLayout VulkanBindingGroupManager::GetLayoutFor(
        const RenderStateConfig& stateConfig)
    {
        const auto hash = stateConfig.GetBindingsHash();
        // Search in the cache for a compatible layout
        if (const auto it = m_SetLayoutCache.find(hash); it != m_SetLayoutCache.end())
        {
            return it->second;
        }

        // If not found, create a new layout
        VAArray<VkDescriptorSetLayoutBinding> vkBindings;
        for (const auto& bindingConfig : stateConfig.expectedBindings)
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
            vkCreateDescriptorSetLayout( m_Device->GetLogicalDeviceHandle(), &layoutInfo,
                m_Allocator, &layout));

        m_SetLayoutCache[hash] = layout;
        return layout;
    }

    void VulkanBindingGroupManager::BindMaterialGroup(
        VkCommandBuffer cmdsBuf,
        MaterialHandle materialHandle,
        RenderStateHandle stateHandle)
    {
        // 1. Check compatibility
        if (!AreLayoutCompatible(materialHandle, stateHandle))
        {
            //TODO: Use a default error material.
            return;
        }

        if (NeedsUpdate(materialHandle))
        {
            UpdateData(materialHandle);
        }

        // 2. Get the descriptor set for this material
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
            if (NeedsUpdate(materialHandle))
            {
                UpdateDescriptorSet(materialSet, materialHandle);
            }
        }

        // 3. Get the RenderState* from the system.
        const auto statePtr = dynamic_cast<VulkanPipeline*>(g_RenderStateSystem->GetPointerFor(
            stateHandle));

        // 4. Bind the descriptor set
        vkCmdBindDescriptorSets(
            cmdsBuf,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            statePtr->GetPipelineLayout(),
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
        const MaterialHandle MaterialHandle,
        const RenderStateHandle stateHandle)
    {
        const auto& materialConfig = g_MaterialSystem->GetTemplateFor(MaterialHandle);
        const auto& stateConfig = g_RenderStateSystem->GetConfigFor(stateHandle);

        if (materialConfig.resourceBindings.size() != stateConfig.expectedBindings.size())
        {
            VA_ENGINE_ERROR("[VulkanBindingGroupManager] Incompatible layout.");
            return false;
        }

        //TODO: We should compare every member.
        const auto materialBindingHash = materialConfig.GetBindingsHash();
        const auto stateBindingHash = stateConfig.GetBindingsHash();
        if (materialBindingHash != stateBindingHash)
        {
            VA_ENGINE_ERROR("[VulkanBindingGroupManager] Incompatible layout.");
            return false;
        }

        return true;
    }

    VkDescriptorSetLayout VulkanBindingGroupManager::GetHandleFor(
        const MaterialHandle materialHandle)
    {
        const auto& materialConfig = g_MaterialSystem->GetTemplateFor(materialHandle);
        const size_t hash = materialConfig.GetBindingsHash();

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
            vkCreateDescriptorSetLayout( m_Device->GetLogicalDeviceHandle(), &layoutInfo,
                m_Allocator, &layout));

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
        auto* vkMat = dynamic_cast<VulkanMaterial*>(g_MaterialSystem->GetPointerFor(
            materialHandle));

        if (!vkMat)
        {
            VA_ENGINE_ERROR("[VulkanBindingGroupManager] Failed to get vulkan material.");
            return;
        }

        // Pointer storage must live until the end
        VAArray<VkWriteDescriptorSet> descriptorWrites;
        VkDescriptorBufferInfo bufferInfo;
        VAArray<VkDescriptorImageInfo> imageInfos;
        uint32_t imageCount = 0;
        for (const auto& bindingConfig : materialConfig.resourceBindings)
        {
            if (bindingConfig.type == Renderer::ResourceBindingType::Texture2D)
            {
                imageCount++;
            }
        }
        imageInfos.reserve(imageCount); // Diffuse, Specular and Normal for now

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

                auto textureHandle = Resources::InvalidTextureHandle;
                //TODO: We could probably refactor the way we define resource binding
                //  to allow a more dynamic binding <=> texture use mapping.
                switch (bindingConfig.binding)
                {
                    case 1:
                        textureHandle = vkMat->GetTexture(Resources::TextureUse::Diffuse);
                        break;
                    case 2:
                        textureHandle = vkMat->GetTexture(Resources::TextureUse::Specular);
                        break;
                    case 3:
                        textureHandle = vkMat->GetTexture(Resources::TextureUse::Normal);
                        break;
                    //TODO: Add here other texture mapping if needed.
                    default: VA_ENGINE_WARN(
                            "[VulkanBindingGroupManager] Unsupported texture binding.");
                        break;
                }

                if (textureHandle == Resources::InvalidTextureHandle)
                {
                    VA_ENGINE_ERROR("[VulkanBindingGroupManager] Invalid texture handle.");
                    textureHandle = g_TextureSystem->GetErrorTextureHandle();
                }

                auto vkTexture = dynamic_cast<VulkanTexture2D*>(g_TextureSystem->GetPointerFor(
                    textureHandle));
                if (!vkTexture)
                {
                    VA_ENGINE_ERROR("[VulkanBindingGroupManager] Invalid texture.");
                    vkTexture = dynamic_cast<VulkanTexture2D*>(g_TextureSystem->GetPointerFor(
                        g_TextureSystem->GetErrorTextureHandle()));
                    if (!vkTexture)
                    {
                        VA_ENGINE_CRITICAL(
                            "[VulkanBindingGroupManager] Failed to get default texture.");
                        throw std::runtime_error("Failed to get default texture.");
                    }
                }

                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = vkTexture->GetImageView();
                imageInfo.sampler = vkTexture->GetSampler();

                imageInfos.push_back(imageInfo);
                write.pImageInfo = &imageInfos.back();
            }

            descriptorWrites.push_back(write);
        }

        vkUpdateDescriptorSets(
            m_Device->GetLogicalDeviceHandle(),
            static_cast<uint32_t>(descriptorWrites.size()),
            descriptorWrites.data(),
            0,
            nullptr);

        vkMat->MarkResourcesUpdated();
    }

    bool VulkanBindingGroupManager::NeedsUpdate(const MaterialHandle materialHandle)
    {
        const auto* mat = g_MaterialSystem->GetPointerFor(materialHandle);
        if (!mat) return false;

        // Check if material generation changed (properties like diffuse color)
        const auto currentGen = mat->GetGeneration();
        bool materialChanged = false;
        if (m_MaterialGenerations.contains(materialHandle))
        {
            materialChanged = currentGen != m_MaterialGenerations[materialHandle];
        }
        else
        {
            materialChanged = true;
        }

        bool resourcesChanged = mat->HasResourcesChanged();

        return materialChanged || resourcesChanged;
    }

    void VulkanBindingGroupManager::UpdateData(MaterialHandle materialHandle)
    {
        auto* mat = g_MaterialSystem->GetPointerFor(materialHandle);
        if (!mat) return;

        auto* vkMat = dynamic_cast<VulkanMaterial*>(mat);
        if (!vkMat) return;

        UpdateMaterialUBO(materialHandle, vkMat->GetUniformData());
        m_MaterialGenerations[materialHandle] = mat->GetGeneration();
    }
}
