//
// Created by Michael Desmedt on 14/05/2025.
//
#include "VulkanRhi.hpp"

#include "Core/Logger.hpp"
#include "Core/Window.hpp"

#include <SDL3/SDL_vulkan.h>

#include "VulkanBindingGroupManager.hpp"
#include "VulkanDevice.hpp"
#include "VulkanExecutionContext.hpp"
#include "VulkanRenderTargetSystem.hpp"
#include "VulkanResourceFactory.hpp"

namespace VoidArchitect::Platform
{
    VulkanRHI::VulkanRHI(
        std::unique_ptr<Window>& window)
        : m_Window(window),
          m_Allocator(nullptr)
    {
        // NOTE Currently we don't provide an allocator, but we might want to do it in the future
        //  that's why there's m_Allocator already.
        m_Allocator = nullptr;

        CreateDevice();
        CreateResourceFactory();
        g_VkRenderTargetSystem = std::make_unique<VulkanRenderTargetSystem>(m_ResourceFactory);

        CreateExecutionContext(m_Window->GetWidth(), m_Window->GetHeight());
        CreateBindingGroupsManager();
    }

    VulkanRHI::~VulkanRHI()
    {
        m_Device->WaitIdle();

        m_BindingGroupsManager = nullptr;
        m_ExecutionContext = nullptr;
        g_VkRenderTargetSystem = nullptr;
        m_ResourceFactory = nullptr;

        m_Device = nullptr;
        VA_ENGINE_INFO("[VulkanRHI] Device destroyed.");
    }

    void VulkanRHI::CreateDevice()
    {
        // TODO DeviceRequirements should be configurable by the Application, but for now we will
        //  keep it simple and make it directly here.
        DeviceRequirements requirements;
        // NOTE With Mx chips from Apple, we should add the Portability extension, there's probably
        //  other extensions needed for other GPUs, so for now I will just add them here for my GPU
        //  but we need a more robust solution.
        requirements.Extensions = {
            // Swapchain is needed for graphics
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            // Portability subset must be enabled on Mx chips.
            "VK_KHR_portability_subset",
        };
        m_Device = std::make_unique<VulkanDevice>(m_Allocator, m_Window, requirements);
        VA_ENGINE_INFO("[VulkanRHI] Device created.");
    }

    void VulkanRHI::CreateExecutionContext(uint32_t width, uint32_t height)
    {
        m_ExecutionContext = std::make_unique<VulkanExecutionContext>(
            m_Device,
            m_ResourceFactory,
            m_Allocator,
            width,
            height);
        VA_ENGINE_INFO("[VulkanRHI] Execution context created.");
    }

    void VulkanRHI::CreateResourceFactory()
    {
        m_ResourceFactory = std::make_unique<VulkanResourceFactory>(m_Device, m_Allocator);
        VA_ENGINE_INFO("[VulkanRHI] Resource factory created.");
    }

    void VulkanRHI::CreateBindingGroupsManager()
    {
        m_BindingGroupsManager = std::make_unique<VulkanBindingGroupManager>(m_Device, m_Allocator);
        VA_ENGINE_INFO("[VulkanRHI] Binding groups manager created.");
    }

    void VulkanRHI::Resize(const uint32_t width, const uint32_t height)
    {
        m_ExecutionContext->RequestResize(width, height);
    }

    bool VulkanRHI::BeginFrame(const float deltaTime)
    {
        return m_ExecutionContext->BeginFrame(deltaTime);
    }

    bool VulkanRHI::EndFrame(const float deltaTime)
    {
        return m_ExecutionContext->EndFrame(deltaTime);
    }

    void VulkanRHI::BeginRenderPass(
        const RenderPassHandle passHandle,
        const VAArray<Resources::RenderTargetHandle>& targetHandles)
    {
        m_ExecutionContext->BeginRenderPass(passHandle, targetHandles);
    }

    void VulkanRHI::EndRenderPass()
    {
        m_ExecutionContext->EndRenderPass();
    }

    void VulkanRHI::UpdateGlobalState(const Resources::GlobalUniformObject& gUBO)
    {
        m_ExecutionContext->UpdateGlobalState(gUBO);
    }

    void VulkanRHI::BindGlobalState(const Resources::RenderStatePtr& pipeline)
    {
        m_ExecutionContext->BindGlobalState(pipeline);
    }

    Resources::Texture2D* VulkanRHI::CreateTexture2D(
        const std::string& name,
        const uint32_t width,
        const uint32_t height,
        const uint8_t channels,
        const bool hasTransparency,
        const VAArray<uint8_t>& data)
    {
        return m_ResourceFactory->CreateTexture2D(
            name,
            width,
            height,
            channels,
            hasTransparency,
            data);
    }

    Resources::IRenderState* VulkanRHI::CreateRenderState(
        RenderStateConfig& config,
        const RenderPassHandle passHandle)
    {
        return m_ResourceFactory->CreateRenderState(config, passHandle);
    }

    Resources::IMaterial* VulkanRHI::CreateMaterial(const std::string& name)
    {
        return m_ResourceFactory->CreateMaterial(name);
    }

    Resources::IShader* VulkanRHI::CreateShader(
        const std::string& name,
        const ShaderConfig& config,
        const VAArray<uint8_t>& data)
    {
        return m_ResourceFactory->CreateShader(name, config, data);
    }

    Resources::IMesh* VulkanRHI::CreateMesh(
        const std::string& name,
        const VAArray<Resources::MeshVertex>& vertices,
        const VAArray<uint32_t>& indices)
    {
        return m_ResourceFactory->CreateMesh(name, vertices, indices);
    }

    Resources::RenderTargetHandle VulkanRHI::CreateRenderTarget(
        const Renderer::RenderTargetConfig& config)
    {
        return g_VkRenderTargetSystem->CreateRenderTarget(config);
    }

    void VulkanRHI::ReleaseRenderTarget(const Resources::RenderTargetHandle handle)
    {
        g_VkRenderTargetSystem->ReleaseRenderTarget(handle);
    }

    Resources::RenderTargetHandle VulkanRHI::GetCurrentColorRenderTargetHandle() const
    {
        return m_ExecutionContext->GetCurrentColorRenderTargetHandle();
    }

    Resources::RenderTargetHandle VulkanRHI::GetDepthRenderTargetHandle() const
    {
        return m_ExecutionContext->GetDepthRenderTargetHandle();
    }

    Resources::IRenderPass* VulkanRHI::CreateRenderPass(
        const RenderPassConfig& config,
        const Renderer::PassPosition passPosition)
    {
        const auto swapchainFormat = m_ExecutionContext->GetSwapchainFormat();
        const auto depthFormat = m_ExecutionContext->GetDepthFormat();
        return m_ResourceFactory->CreateRenderPass(
            config,
            passPosition,
            swapchainFormat,
            depthFormat);
    }
} // namespace VoidArchitect::Platform
