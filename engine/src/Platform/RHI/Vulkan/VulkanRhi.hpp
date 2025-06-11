//
// Created by Michael Desmedt on 14/05/2025.
//
#pragma once
#include <vulkan/vulkan.h>

#include "Platform/RHI/IRenderingHardware.hpp"
#include "Resources/Material.hpp"
#include "Systems/RenderStateSystem.hpp"

namespace VoidArchitect
{
    class Window;
} // namespace VoidArchitect

namespace VoidArchitect::Platform
{
    class VulkanBindingGroupManager;
    class VulkanDevice;
    class VulkanRenderTarget;
    class VulkanResourceFactory;
    class VulkanExecutionContext;

    class VulkanRHI final : public IRenderingHardware
    {
    public:
        explicit VulkanRHI(std::unique_ptr<Window>& window);
        ~VulkanRHI() override;

        void Resize(uint32_t width, uint32_t height) override;
        void WaitIdle() override;

        bool BeginFrame(float deltaTime) override;
        bool EndFrame(float deltaTime) override;

        void BeginRenderPass(
            RenderPassHandle passHandle,
            const VAArray<Resources::RenderTargetHandle>& targetHandles) override;
        void EndRenderPass() override;

        void UpdateGlobalState(const Resources::GlobalUniformObject& gUBO) override;
        void BindGlobalState(const Resources::RenderStatePtr& pipeline) override;

        void BindMaterial(MaterialHandle materialHandle, RenderStateHandle stateHandle) override;

        ///////////////////////////////////////////////////////////////////////
        //// Resources ////////////////////////////////////////////////////////
        ///////////////////////////////////////////////////////////////////////
        Resources::Texture2D* CreateTexture2D(
            const std::string& name,
            uint32_t width,
            uint32_t height,
            uint8_t channels,
            bool hasTransparency,
            const VAArray<uint8_t>& data) override;

        Resources::IRenderState* CreateRenderState(
            RenderStateConfig& config,
            RenderPassHandle passHandle) override;

        Resources::IMaterial* CreateMaterial(
            const std::string& name,
            const MaterialTemplate& matTemplate) override;

        Resources::IShader* CreateShader(
            const std::string& name,
            const ShaderConfig& config,
            const VAArray<uint8_t>& data) override;

        Resources::IMesh* CreateMesh(
            const std::string& name,
            const VAArray<Resources::MeshVertex>& vertices,
            const VAArray<uint32_t>& indices) override;

        Resources::RenderTargetHandle CreateRenderTarget(
            const Renderer::RenderTargetConfig& config) override;

        void ReleaseRenderTarget(Resources::RenderTargetHandle handle) override;

        Resources::RenderTargetHandle GetCurrentColorRenderTargetHandle() const override;

        Resources::RenderTargetHandle GetDepthRenderTargetHandle() const override;

        Resources::IRenderPass* CreateRenderPass(
            const Renderer::RenderPassConfig& config,
            Renderer::PassPosition passPosition) override;

        std::unique_ptr<VulkanDevice>& GetDeviceRef() { return m_Device; }

    private:
        void CreateDevice();
        void CreateExecutionContext(uint32_t width, uint32_t height);
        void CreateResourceFactory();
        void CreateBindingGroupsManager();

        std::unique_ptr<Window>& m_Window;

        VkAllocationCallbacks* m_Allocator;
        std::unique_ptr<VulkanDevice> m_Device;

        VAArray<VulkanRenderTarget*> m_MainRenderTargets;
    };
} // namespace VoidArchitect::Platform
