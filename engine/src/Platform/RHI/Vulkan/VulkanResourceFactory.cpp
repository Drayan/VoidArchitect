//
// Created by Michael Desmedt on 08/06/2025.
//
#include "VulkanResourceFactory.hpp"

#include "VulkanMaterial.hpp"
#include "VulkanMesh.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanRenderPass.hpp"
#include "VulkanRenderTarget.hpp"
#include "VulkanShader.hpp"
#include "VulkanTexture.hpp"
#include "Core/Logger.hpp"
#include "Systems/Renderer/RenderGraph.hpp"

namespace VoidArchitect::Platform
{
    VulkanResourceFactory::VulkanResourceFactory(
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator)
        : m_Device(device),
          m_Allocator(allocator)
    {
    }

    Resources::Texture2D* VulkanResourceFactory::CreateTexture2D(
        const std::string& name,
        const uint32_t width,
        const uint32_t height,
        const uint8_t channels,
        const bool hasTransparency,
        const VAArray<uint8_t>& data) const
    {
        return new VulkanTexture2D(
            m_Device,
            m_Allocator,
            name,
            width,
            height,
            channels,
            hasTransparency,
            data);
    }

    Resources::IRenderState* VulkanResourceFactory::CreateRenderState(
        const RenderStateConfig& config,
        const RenderPassHandle passHandle) const
    {
        // Ask the RenderPassSystem for the IRenderPass*
        const auto renderPassPtr = g_RenderPassSystem->GetPointerFor(passHandle);
        if (!renderPassPtr)
        {
            VA_ENGINE_ERROR("[VulkanRHI] Invalid render pass handle.");
            return nullptr;
        }
        const auto vkRenderPass = dynamic_cast<VulkanRenderPass*>(renderPassPtr);
        if (!vkRenderPass)
        {
            VA_ENGINE_ERROR("[VulkanRHI] Invalid render pass type.");
            return nullptr;
        }

        return new VulkanPipeline(config, m_Device, m_Allocator, vkRenderPass);
    }

    Resources::IMaterial* VulkanResourceFactory::CreateMaterial(const std::string& name) const
    {
        return new VulkanMaterial(name, m_Device, m_Allocator);
    }

    Resources::IShader* VulkanResourceFactory::CreateShader(
        const std::string& name,
        const ShaderConfig& config,
        const VAArray<uint8_t>& data) const
    {
        return new VulkanShader(m_Device, m_Allocator, name, config, data);
    }

    Resources::IMesh* VulkanResourceFactory::CreateMesh(
        const std::string& name,
        const VAArray<Resources::MeshVertex>& vertices,
        const VAArray<uint32_t>& indices) const
    {
        return new VulkanMesh(m_Device, m_Allocator, name, vertices, indices);
    }

    Resources::IRenderTarget* VulkanResourceFactory::CreateRenderTarget(
        const Renderer::RenderTargetConfig& config) const
    {
        VkImageUsageFlags usageFlags = 0;
        VkImageAspectFlags aspectFlags = 0;

        if (static_cast<uint32_t>(config.usage) & static_cast<uint32_t>(
            Renderer::RenderTargetUsage::ColorAttachment))
        {
            usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            aspectFlags |= VK_IMAGE_ASPECT_COLOR_BIT;
        }
        if (static_cast<uint32_t>(config.usage) & static_cast<uint32_t>(
            Renderer::RenderTargetUsage::DepthStencilAttachment))
        {
            usageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            aspectFlags |= VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        if (static_cast<uint32_t>(config.usage) & static_cast<uint32_t>(
            Renderer::RenderTargetUsage::RenderTexture))
        {
            usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        }
        usageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        uint32_t width = config.width;
        uint32_t height = config.height;

        if (config.sizingPolicy == Renderer::RenderTargetConfig::SizingPolicy::RelativeToViewport)
        {
            //TODO Get swapchain width and height
        }

        auto image = VulkanImage(
            m_Device,
            m_Allocator,
            width,
            height,
            TranslateEngineTextureFormatToVulkan(config.format),
            aspectFlags,
            VK_IMAGE_TILING_OPTIMAL,
            usageFlags,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        return new VulkanRenderTarget(config.name, std::move(image));
    }

    Resources::IRenderTarget* VulkanResourceFactory::CreateRenderTarget(
        const std::string& name,
        const VkImage nativeImage,
        const VkFormat format) const
    {
        auto image = VulkanImage(
            m_Device,
            m_Allocator,
            nativeImage,
            format,
            VK_IMAGE_ASPECT_COLOR_BIT);

        return new VulkanRenderTarget(name, std::move(image));
    }

    Resources::IRenderPass* VulkanResourceFactory::CreateRenderPass(
        const RenderPassConfig& config,
        const Renderer::PassPosition passPosition,
        const VkFormat swapchainFormat,
        const VkFormat depthFormat) const
    {
        return new VulkanRenderPass(
            config,
            m_Device,
            m_Allocator,
            passPosition,
            swapchainFormat,
            depthFormat);
    }
}
