//
// Created by Michael Desmedt on 20/05/2025.
//
#include "VulkanMaterial.hpp"

#include "VulkanBuffer.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanShader.hpp"
#include "VulkanSwapchain.hpp"
#include "VulkanUtils.hpp"
#include "VulkanCommandBuffer.hpp"

namespace VoidArchitect::Platform
{
    constexpr std::string BUILTIN_OBJECT_SHADER_NAME = "BuiltinObject";

    VulkanMaterial::VulkanMaterial(
        const VulkanRHI& rhi,
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        const std::unique_ptr<VulkanSwapchain>& swapchain,
        const std::unique_ptr<VulkanRenderpass>& renderpass)
        : m_Allocator(allocator), m_Device(device->GetLogicalDeviceHandle())
    {
        // --- Load Builtin shaders ---
        m_Shaders.reserve(2);
        m_Shaders.emplace_back(
            device,
            m_Allocator,
            ShaderStage::Vertex,
            BUILTIN_OBJECT_SHADER_NAME + ".vert");
        m_Shaders.emplace_back(
            device,
            m_Allocator,
            ShaderStage::Pixel,
            BUILTIN_OBJECT_SHADER_NAME + ".pixl");

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

        VA_VULKAN_CHECK_RESULT_WARN(
            vkCreateDescriptorSetLayout(
                m_Device,
                &globalLayoutInfo,
                m_Allocator,
                &m_DescriptorSetLayout));

        auto globalPoolSize = VkDescriptorPoolSize{};
        globalPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        globalPoolSize.descriptorCount = swapchain->GetImageCount();

        auto globalPoolInfo = VkDescriptorPoolCreateInfo{};
        globalPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        globalPoolInfo.maxSets = swapchain->GetImageCount();
        globalPoolInfo.poolSizeCount = 1;
        globalPoolInfo.pPoolSizes = &globalPoolSize;

        VA_VULKAN_CHECK_RESULT_WARN(
            vkCreateDescriptorPool(
                m_Device,
                &globalPoolInfo,
                m_Allocator,
                &m_DescriptorPool));

        // --- Attributes ---
        std::vector attributes = {
            // Position
            VkVertexInputAttributeDescription{
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 0
            }
        };

        // --- Descriptors set layouts ---
        std::vector descriptorSetLayouts = {
            m_DescriptorSetLayout
        };

        // --- Create the graphics pipeline ---
        m_Pipeline = std::make_unique<VulkanPipeline>(
            device,
            m_Allocator,
            renderpass,
            m_Shaders,
            attributes,
            descriptorSetLayouts);

        m_GlobalUniformBuffer = std::make_unique<VulkanBuffer>(
            rhi,
            device,
            m_Allocator,
            sizeof(GlobalUniformObject) * 3,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        const std::vector globalLayouts = {
            m_DescriptorSetLayout,
            m_DescriptorSetLayout,
            m_DescriptorSetLayout
        };

        m_DescriptorSets = new VkDescriptorSet[globalLayouts.size()];

        auto allocInfo = VkDescriptorSetAllocateInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_DescriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(globalLayouts.size());
        allocInfo.pSetLayouts = globalLayouts.data();
        VA_VULKAN_CHECK_RESULT_WARN(
            vkAllocateDescriptorSets(
                m_Device,
                &allocInfo,
                m_DescriptorSets)
        );

        VA_ENGINE_INFO("[VulkanMaterial] Material created.");
    }

    VulkanMaterial::~VulkanMaterial()
    {
        delete[] m_DescriptorSets;
        VA_ENGINE_DEBUG("[VulkanMaterial] Descriptor sets destroyed.");

        m_GlobalUniformBuffer.reset();
        VA_ENGINE_DEBUG("[VulkanMaterial] Global uniform buffer destroyed.");

        m_Pipeline.reset();
        VA_ENGINE_DEBUG("[VulkanMaterial] Pipeline destroyed.");

        vkDestroyDescriptorPool(m_Device, m_DescriptorPool, m_Allocator);
        VA_ENGINE_DEBUG("[VulkanMaterial] Descriptor pool destroyed.");

        vkDestroyDescriptorSetLayout(
            m_Device,
            m_DescriptorSetLayout,
            m_Allocator);
        VA_ENGINE_DEBUG("[VulkanMaterial] Descriptor set layout destroyed.");
        VA_ENGINE_INFO("[VulkanMaterial] Material destroyed.");
    }

    void VulkanMaterial::Use(IRenderingHardware& rhi)
    {
        auto& vulkanRhi = dynamic_cast<VulkanRHI&>(rhi);
        m_Pipeline->Bind(vulkanRhi.GetCurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS);
    }

    void VulkanMaterial::SetGlobalUniforms(
        IRenderingHardware& rhi,
        const Math::Mat4& projection,
        const Math::Mat4& view)
    {
        auto& vulkanRhi = dynamic_cast<VulkanRHI&>(rhi);
        const auto imageIndex = vulkanRhi.GetImageIndex();
        const auto& globalDescriptor = m_DescriptorSets[imageIndex];

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

        vkUpdateDescriptorSets(
            m_Device,
            1,
            &writeDescriptorSet,
            0,
            nullptr);

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

    void VulkanMaterial::SetObjectModelConstant(IRenderingHardware& rhi, const Math::Mat4& model)
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
            &model);
    }
}
