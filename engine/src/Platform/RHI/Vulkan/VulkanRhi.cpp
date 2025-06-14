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
    VulkanRHI::VulkanRHI(std::unique_ptr<Window>& window)
        : m_Window(window),
          m_Allocator(nullptr)
    {
        // NOTE Currently we don't provide an allocator, but we might want to do it in the future
        //  that's why there's m_Allocator already.
        m_Allocator = nullptr;

        CreateDevice();
        CreateResourceFactory();
        g_VkRenderTargetSystem = std::make_unique<VulkanRenderTargetSystem>(g_VkResourceFactory);

        CreateExecutionContext(m_Window->GetWidth(), m_Window->GetHeight());
        CreateBindingGroupsManager();
    }

    VulkanRHI::~VulkanRHI()
    {
        m_Device->WaitIdle();

        g_VkBindingGroupManager = nullptr;
        g_VkExecutionContext = nullptr;
        g_VkRenderTargetSystem = nullptr;
        g_VkResourceFactory = nullptr;

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
        g_VkExecutionContext = std::make_unique<VulkanExecutionContext>(
            m_Device,
            m_Allocator,
            width,
            height);
        VA_ENGINE_INFO("[VulkanRHI] Execution context created.");
    }

    void VulkanRHI::CreateResourceFactory()
    {
        g_VkResourceFactory = std::make_unique<VulkanResourceFactory>(m_Device, m_Allocator);
        VA_ENGINE_INFO("[VulkanRHI] Resource factory created.");
    }

    void VulkanRHI::CreateBindingGroupsManager()
    {
        g_VkBindingGroupManager = std::make_unique<
            VulkanBindingGroupManager>(m_Device, m_Allocator);
        VA_ENGINE_INFO("[VulkanRHI] Binding groups manager created.");
    }

    void VulkanRHI::Resize(const uint32_t width, const uint32_t height)
    {
        g_VkExecutionContext->RequestResize(width, height);
    }

    void VulkanRHI::WaitIdle()
    {
        m_Device->WaitIdle();
    }

    bool VulkanRHI::BeginFrame(const float deltaTime)
    {
        return g_VkExecutionContext->BeginFrame(deltaTime);
    }

    bool VulkanRHI::EndFrame(const float deltaTime)
    {
        return g_VkExecutionContext->EndFrame(deltaTime);
    }

    void VulkanRHI::BeginRenderPass(
        const RenderPassHandle passHandle,
        const VAArray<Resources::RenderTargetHandle>& targetHandles)
    {
        g_VkExecutionContext->BeginRenderPass(passHandle, targetHandles);
    }

    void VulkanRHI::EndRenderPass()
    {
        g_VkExecutionContext->EndRenderPass();
    }

    void VulkanRHI::UpdateGlobalState(const Resources::GlobalUniformObject& gUBO)
    {
        g_VkExecutionContext->UpdateGlobalState(gUBO);
    }

    void VulkanRHI::BindRenderState(RenderStateHandle stateHandle)
    {
        g_VkExecutionContext->BindRenderState(stateHandle);
    }

    void VulkanRHI::BindMaterial(
        const MaterialHandle materialHandle,
        const RenderStateHandle stateHandle)
    {
        g_VkExecutionContext->BindMaterialGroup(materialHandle, stateHandle);
    }

    void VulkanRHI::BindMesh(Resources::MeshHandle meshHandle)
    {
        g_VkExecutionContext->BindMesh(meshHandle);
    }

    auto VulkanRHI::DrawIndexed(
        const uint32_t indexCount,
        const uint32_t indexOffset,
        const uint32_t vertexOffset,
        const uint32_t instanceCount,
        const uint32_t firstInstance) -> void
    {
        g_VkExecutionContext->DrawIndexed(
            indexCount,
            indexOffset,
            vertexOffset,
            instanceCount,
            firstInstance);
    }

    void VulkanRHI::PushConstants(
        const Resources::ShaderStage stage,
        const uint32_t size,
        const void* data)
    {
        g_VkExecutionContext->PushConstants(stage, size, data);
    }

    Resources::Texture2D* VulkanRHI::CreateTexture2D(
        const std::string& name,
        const uint32_t width,
        const uint32_t height,
        const uint8_t channels,
        const bool hasTransparency,
        const VAArray<uint8_t>& data)
    {
        return g_VkResourceFactory->CreateTexture2D(
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
        return g_VkResourceFactory->CreateRenderState(config, passHandle);
    }

    Resources::IMaterial* VulkanRHI::CreateMaterial(
        const std::string& name,
        const MaterialTemplate& matTemplate)
    {
        return g_VkResourceFactory->CreateMaterial(name, matTemplate);
    }

    Resources::IShader* VulkanRHI::CreateShader(
        const std::string& name,
        const ShaderConfig& config,
        const VAArray<uint8_t>& data)
    {
        return g_VkResourceFactory->CreateShader(name, config, data);
    }

    Resources::IMesh* VulkanRHI::CreateMesh(
        const std::string& name,
        const std::shared_ptr<Resources::MeshData>& data,
        const VAArray<Resources::SubMeshDescriptor>& submeshes)
    {
        return g_VkResourceFactory->CreateMesh(name, data, submeshes);
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
        return g_VkExecutionContext->GetCurrentColorRenderTargetHandle();
    }

    Resources::RenderTargetHandle VulkanRHI::GetDepthRenderTargetHandle() const
    {
        return g_VkExecutionContext->GetDepthRenderTargetHandle();
    }

    Resources::IRenderPass* VulkanRHI::CreateRenderPass(
        const Renderer::RenderPassConfig& config,
        const Renderer::PassPosition passPosition)
    {
        const auto swapchainFormat = g_VkExecutionContext->GetSwapchainFormat();
        const auto depthFormat = g_VkExecutionContext->GetDepthFormat();
        return g_VkResourceFactory->CreateRenderPass(
            config,
            passPosition,
            swapchainFormat,
            depthFormat);
    }
} // namespace VoidArchitect::Platform
