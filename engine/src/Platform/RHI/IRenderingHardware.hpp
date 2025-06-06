//
// Created by Michael Desmedt on 14/05/2025.
//
#pragma once
#include "IRenderingHardware.hpp"
#include "Resources/Mesh.hpp"

namespace VoidArchitect
{
    struct RenderPassConfig;
    struct ShaderConfig;
    struct RenderStateConfig;

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

    using RenderStatePtr = std::shared_ptr<IRenderState>;
} // namespace VoidArchitect::Resources

namespace VoidArchitect::Platform
{
    enum class RHI_API_TYPE
    {
        None = 0, // Headless application, e.g., for the server.
        Vulkan, // Vulkan implementation of RHI.
    };

    class IRenderingHardware
    {
    public:
        IRenderingHardware() = default;
        virtual ~IRenderingHardware() = default;

        virtual void Resize(uint32_t width, uint32_t height) = 0;
        virtual void WaitIdle(uint64_t timeout) = 0;

        virtual bool BeginFrame(float deltaTime) = 0;
        virtual bool EndFrame(float deltaTime) = 0;

        virtual void UpdateGlobalState(const Resources::GlobalUniformObject& gUBO) = 0;
        virtual void BindGlobalState(const Resources::RenderStatePtr& pipeline) = 0;

        virtual void DrawMesh(
            const Resources::GeometryRenderData& data,
            const Resources::RenderStatePtr& pipeline) = 0;

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
        virtual Resources::IRenderState* CreatePipeline(
            RenderStateConfig& config,
            Resources::IRenderPass* renderPass) = 0;
        virtual Resources::IMaterial* CreateMaterial(const std::string& name) = 0;
        virtual Resources::IShader* CreateShader(
            const std::string& name,
            const ShaderConfig& config,
            const VAArray<uint8_t>& data) = 0;
        virtual Resources::IMesh* CreateMesh(
            const std::string& name,
            const VAArray<Resources::MeshVertex>& vertices,
            const VAArray<uint32_t>& indices) = 0;

        ///////////////////////////////////////////////////////////////////////
        //// RenderGraph Resources ////////////////////////////////////////////
        ///////////////////////////////////////////////////////////////////////
        virtual Resources::IRenderTarget* CreateRenderTarget(
            const Renderer::RenderTargetConfig& config) = 0;
        virtual Resources::IRenderPass* CreateRenderPass(
            const RenderPassConfig& config,
            Renderer::PassPosition passPosition) = 0;
    };
} // namespace VoidArchitect::Platform
