//
// Created by Michael Desmedt on 14/05/2025.
//
#pragma once
#include "../Resources/Material.hpp"
#include "../Resources/Mesh.hpp"
#include "../Resources/RenderTarget.hpp"
#include "../Resources/RenderPass.hpp"
#include "../Resources/RenderState.hpp"

namespace VoidArchitect
{
    struct MaterialTemplate;
    struct RenderPassConfig;

    namespace Renderer
    {
        struct RenderTargetConfig;
        enum class PassPosition;
    } // namespace Renderer
} // namespace VoidArchitect

namespace VoidArchitect::Math
{
    class Mat4;
}

namespace VoidArchitect::Resources
{
    struct GeometryRenderData;
    class Texture2D;
    class IMaterial;
    class IShader;
    class IRenderState;
    class IRenderTarget;
    class IRenderPass;
    struct GlobalUniformObject;
    struct ShaderConfig;

    using RenderStatePtr = std::shared_ptr<IRenderState>;
} // namespace VoidArchitect::Resources

namespace VoidArchitect::Platform
{
    enum class RHI_API_TYPE
    {
        None = 0, // Headless application, e.g., for the server.
        Vulkan, // Vulkan implementation of RHI.
        DirectX12,
        OpenGL,
        Metal,
    };

    class IRenderingHardware
    {
    public:
        IRenderingHardware() = default;
        virtual ~IRenderingHardware() = default;

        virtual void Resize(uint32_t width, uint32_t height) = 0;
        virtual void WaitIdle() = 0;

        virtual bool BeginFrame(float deltaTime) = 0;
        virtual bool EndFrame(float deltaTime) = 0;

        virtual void BeginRenderPass(
            Resources::RenderPassHandle passHandle,
            const VAArray<Resources::RenderTargetHandle>& targetHandles) = 0;
        virtual void EndRenderPass() = 0;

        virtual void UpdateGlobalState(const Resources::GlobalUniformObject& gUBO) = 0;

        virtual void BindRenderState(Resources::RenderStateHandle stateHandle) = 0;
        virtual void BindMaterial(
            MaterialHandle materialHandle,
            Resources::RenderStateHandle stateHandle) = 0;
        virtual bool BindMesh(Resources::MeshHandle meshHandle) = 0;
        virtual void PushConstants(
            Resources::ShaderStage stage,
            uint32_t size,
            const void* data) = 0;
        virtual void DrawIndexed(
            uint32_t indexCount,
            uint32_t indexOffset = 0,
            uint32_t vertexOffset = 0,
            uint32_t instanceCount = 1,
            uint32_t firstInstance = 0) = 0;

        ///////////////////////////////////////////////////////////////////////
        //// Resources ////////////////////////////////////////////////////////
        ///////////////////////////////////////////////////////////////////////
        virtual Resources::Texture2D* CreateTexture2D(
            const std::string& name,
            uint32_t width,
            uint32_t height,
            uint8_t channels,
            bool hasTransparency,
            const VAArray<uint8_t>& data) = 0;

        virtual Resources::IRenderState* CreateRenderState(
            Resources::RenderStateConfig& config,
            Resources::RenderPassHandle passHandle) = 0;

        virtual Resources::IMaterial* CreateMaterial(
            const std::string& name,
            const MaterialTemplate& matTemplate) = 0;

        virtual Resources::IShader* CreateShader(
            const std::string& name,
            const Resources::ShaderConfig& config,
            const VAArray<uint8_t>& data) = 0;

        virtual Resources::IMesh* CreateMesh(
            const std::string& name,
            const std::shared_ptr<Resources::MeshData>& data,
            const VAArray<Resources::SubMeshDescriptor>& submeshes) = 0;

        virtual Resources::RenderTargetHandle CreateRenderTarget(
            const Renderer::RenderTargetConfig& config) = 0;

        virtual void ReleaseRenderTarget(Resources::RenderTargetHandle handle) = 0;

        virtual Resources::RenderTargetHandle GetCurrentColorRenderTargetHandle() const = 0;
        virtual Resources::RenderTargetHandle GetDepthRenderTargetHandle() const = 0;

        virtual Resources::IRenderPass* CreateRenderPass(
            const Renderer::RenderPassConfig& config,
            Renderer::PassPosition passPosition) = 0;
    };
} // namespace VoidArchitect::Platform
