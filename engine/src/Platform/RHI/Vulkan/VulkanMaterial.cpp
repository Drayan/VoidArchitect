//
// Created by Michael Desmedt on 20/05/2025.
//
#include "VulkanMaterial.hpp"

#include <ranges>

#include "Core/Math/Vec3.hpp"
#include "VulkanBuffer.hpp"
#include "VulkanCommandBuffer.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanShader.hpp"
#include "VulkanSwapchain.hpp"
#include "VulkanTexture.hpp"
#include "VulkanUtils.hpp"

namespace VoidArchitect::Platform
{
    constexpr std::string BUILTIN_OBJECT_SHADER_NAME = "BuiltinObject";

    VulkanMaterial::VulkanMaterial(
        const VulkanRHI& rhi,
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        const std::unique_ptr<VulkanSwapchain>& swapchain,
        const std::unique_ptr<VulkanRenderpass>& renderpass)
        : m_Allocator(allocator),
          m_Device(device->GetLogicalDeviceHandle()),
          m_GlobalDescriptorPool{},
          m_GlobalDescriptorSetLayout{},
          m_LocalUniformBufferIndex{}
    {
        // --- Load Builtin shaders ---
        m_Shaders.reserve(2);
        m_Shaders.emplace_back(
            device, m_Allocator, ShaderStage::Vertex, BUILTIN_OBJECT_SHADER_NAME + ".vert");
        m_Shaders.emplace_back(
            device, m_Allocator, ShaderStage::Pixel, BUILTIN_OBJECT_SHADER_NAME + ".pixl");

        // --- Global Descriptors ---
        auto globalUBOLayoutBinding = VkDescriptorSetLayoutBinding{};
        globalUBOLayoutBinding.binding = 0;
        globalUBOLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        globalUBOLayoutBinding.descriptorCount = 1;
        globalUBOLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        auto globalLayoutInfo = VkDescriptorSetLayoutCreateInfo{};
        globalLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        globalLayoutInfo.bindingCount = 1;
        globalLayoutInfo.pBindings = &globalUBOLayoutBinding;

        VA_VULKAN_CHECK_RESULT_WARN(vkCreateDescriptorSetLayout(
            m_Device, &globalLayoutInfo, m_Allocator, &m_GlobalDescriptorSetLayout));

        auto globalPoolSize = VkDescriptorPoolSize{};
        globalPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        globalPoolSize.descriptorCount = swapchain->GetImageCount();

        auto globalPoolInfo = VkDescriptorPoolCreateInfo{};
        globalPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        globalPoolInfo.maxSets = swapchain->GetImageCount();
        globalPoolInfo.poolSizeCount = 1;
        globalPoolInfo.pPoolSizes = &globalPoolSize;

        VA_VULKAN_CHECK_RESULT_WARN(vkCreateDescriptorPool(
            m_Device, &globalPoolInfo, m_Allocator, &m_GlobalDescriptorPool));

        // --- Local Descriptors ---
        m_LocalDescriptorTypes = std::vector{
            // Binding 0 - Uniform buffer
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            // Binding 1 - Diffuse sampler layout.
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        };
        auto localDescriptorsBinding = std::vector<VkDescriptorSetLayoutBinding>();
        for (auto type : m_LocalDescriptorTypes)
        {
            auto binding = VkDescriptorSetLayoutBinding{};
            binding.binding = localDescriptorsBinding.size();
            binding.descriptorType = type;
            binding.descriptorCount = 1;
            binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

            localDescriptorsBinding.push_back(binding);
        }

        auto localLayoutInfo = VkDescriptorSetLayoutCreateInfo{};
        localLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        localLayoutInfo.bindingCount = static_cast<uint32_t>(localDescriptorsBinding.size());
        localLayoutInfo.pBindings = localDescriptorsBinding.data();

        VA_VULKAN_CHECK_RESULT_WARN(vkCreateDescriptorSetLayout(
            m_Device, &localLayoutInfo, m_Allocator, &m_LocalDescriptorSetLayout));

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

        // --- Attributes ---
        std::vector attributes = {
            // Position
            VkVertexInputAttributeDescription{
                .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0},
            // UV0
            VkVertexInputAttributeDescription{
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = sizeof(Math::Vec3)}};

        // --- Descriptors set layouts ---
        std::vector descriptorSetLayouts = {
            m_GlobalDescriptorSetLayout,
            m_LocalDescriptorSetLayout,
        };

        // --- Create the graphics pipeline ---
        m_Pipeline = std::make_unique<VulkanPipeline>(
            device, m_Allocator, renderpass, m_Shaders, attributes, descriptorSetLayouts);

        m_GlobalUniformBuffer = std::make_unique<VulkanBuffer>(
            rhi,
            device,
            m_Allocator,
            sizeof(GlobalUniformObject) * 3,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        const std::vector globalLayouts = {
            m_GlobalDescriptorSetLayout, m_GlobalDescriptorSetLayout, m_GlobalDescriptorSetLayout};

        m_GlobalDescriptorSets = new VkDescriptorSet[globalLayouts.size()];

        auto allocInfo = VkDescriptorSetAllocateInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_GlobalDescriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(globalLayouts.size());
        allocInfo.pSetLayouts = globalLayouts.data();
        VA_VULKAN_CHECK_RESULT_WARN(
            vkAllocateDescriptorSets(m_Device, &allocInfo, m_GlobalDescriptorSets));

        m_LocalUniformBuffer = std::make_unique<VulkanBuffer>(
            rhi,
            device,
            m_Allocator,
            sizeof(LocalUniformObject),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VA_ENGINE_INFO("[VulkanMaterial] Material created.");
    }

    VulkanMaterial::~VulkanMaterial()
    {
        // We should release any instances remaining.
        for (const auto& id : m_InstanceLocalStates | std::views::keys)
            ReleaseResources(id);

        m_LocalUniformBuffer = nullptr;
        VA_ENGINE_DEBUG("[VulkanMaterial] Local uniform buffer destroyed.");

        delete[] m_GlobalDescriptorSets;
        VA_ENGINE_DEBUG("[VulkanMaterial] Descriptor sets destroyed.");

        m_GlobalUniformBuffer = nullptr;
        VA_ENGINE_DEBUG("[VulkanMaterial] Global uniform buffer destroyed.");

        m_Pipeline = nullptr;
        VA_ENGINE_DEBUG("[VulkanMaterial] Pipeline destroyed.");

        vkDestroyDescriptorPool(m_Device, m_LocalDescriptorPool, m_Allocator);
        VA_ENGINE_DEBUG("[VulkanMaterial] Descriptor pool destroyed.");

        vkDestroyDescriptorSetLayout(m_Device, m_LocalDescriptorSetLayout, m_Allocator);
        VA_ENGINE_DEBUG("[VulkanMaterial] Descriptor set layout destroyed.");

        vkDestroyDescriptorPool(m_Device, m_GlobalDescriptorPool, m_Allocator);
        VA_ENGINE_DEBUG("[VulkanMaterial] Descriptor pool destroyed.");

        vkDestroyDescriptorSetLayout(m_Device, m_GlobalDescriptorSetLayout, m_Allocator);
        VA_ENGINE_DEBUG("[VulkanMaterial] Descriptor set layout destroyed.");
        VA_ENGINE_INFO("[VulkanMaterial] Material destroyed.");
    }

    void VulkanMaterial::Use(IRenderingHardware& rhi)
    {
        auto& vulkanRhi = dynamic_cast<VulkanRHI&>(rhi);
        m_Pipeline->Bind(vulkanRhi.GetCurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS);
    }

    void VulkanMaterial::SetGlobalUniforms(
        IRenderingHardware& rhi, const Math::Mat4& projection, const Math::Mat4& view)
    {
        auto& vulkanRhi = dynamic_cast<VulkanRHI&>(rhi);
        const auto imageIndex = vulkanRhi.GetImageIndex();
        const auto& globalDescriptor = m_GlobalDescriptorSets[imageIndex];

        const auto globalUniformObject = GlobalUniformObject{
            .Projection = projection,
            .View = view,
            .Reserved0 = Math::Mat4::Identity(),
            .Reserved1 = Math::Mat4::Identity(),
        };
        auto data = std::vector{globalUniformObject, globalUniformObject, globalUniformObject};
        m_GlobalUniformBuffer->LoadData(data);

        // Update the descriptor set
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_GlobalUniformBuffer->GetHandle();
        bufferInfo.offset = sizeof(GlobalUniformObject) * imageIndex;
        bufferInfo.range = sizeof(GlobalUniformObject);

        VkWriteDescriptorSet writeDescriptorSet{};
        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.dstSet = globalDescriptor;
        writeDescriptorSet.dstBinding = 0;
        writeDescriptorSet.dstArrayElement = 0;
        writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(m_Device, 1, &writeDescriptorSet, 0, nullptr);

        const auto& cmdBuf = vulkanRhi.GetCurrentCommandBuffer();
        vkCmdBindDescriptorSets(
            cmdBuf.GetHandle(),
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_Pipeline->GetPipelineLayout(),
            0,
            1,
            &globalDescriptor,
            0,
            nullptr);
    }

    void VulkanMaterial::SetObject(IRenderingHardware& rhi, const GeometryRenderData& data)
    {
        auto& vulkanRhi = dynamic_cast<VulkanRHI&>(rhi);
        const auto& cmdBuf = vulkanRhi.GetCurrentCommandBuffer();

        // NOTE VulkanSpec guarantee 128 bytes.
        vkCmdPushConstants(
            cmdBuf.GetHandle(),
            m_Pipeline->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(Math::Mat4),
            // 64 bytes
            &data.Model);

        // Obtain material data
        if (m_InstanceLocalStates.contains(data.ObjectId))
        {
            auto& instanceState = m_InstanceLocalStates[data.ObjectId];
            auto& localDescriptorSet = instanceState.m_DescriptorSet[vulkanRhi.GetImageIndex()];

            std::vector<VkWriteDescriptorSet> descriptorWrites;

            // Descriptor 0 - Uniform buffer
            auto range = sizeof(LocalUniformObject);
            auto offset = sizeof(LocalUniformObject) * instanceState.m_InstanceIndex;

            auto obo = std::vector{LocalUniformObject{
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
                auto texture = std::dynamic_pointer_cast<VulkanTexture2D>(data.Textures[i]);
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
                m_Pipeline->GetPipelineLayout(),
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
            const VkDescriptorSetLayout layouts[] = {
                m_LocalDescriptorSetLayout, m_LocalDescriptorSetLayout, m_LocalDescriptorSetLayout};

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
} // namespace VoidArchitect::Platform
