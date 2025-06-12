//
// Created by Michael Desmedt on 08/06/2025.
//
#pragma once
#include "VulkanDevice.hpp"
#include "Systems/MaterialSystem.hpp"
#include "Systems/RenderPassSystem.hpp"

namespace VoidArchitect
{
    namespace Renderer
    {
        struct RenderTargetConfig;
    }

    struct ShaderConfig;
    struct RenderStateConfig;

    namespace Resources
    {
        class IRenderTarget;
        struct MeshVertex;
        class IMesh;
        class IShader;
        class Texture2D;
        class IMaterial;
        class IRenderState;
        struct GlobalUniformObject;
    } // namespace Resources

    namespace Platform
    {
        class VulkanBindingGroupManager;
        class VulkanExecutionContext;

        class VulkanResourceFactory
        {
        public:
            VulkanResourceFactory(
                const std::unique_ptr<VulkanDevice>& device,
                VkAllocationCallbacks* allocator);
            ~VulkanResourceFactory() = default;

            Resources::Texture2D* CreateTexture2D(
                const std::string& name,
                uint32_t width,
                uint32_t height,
                uint8_t channels,
                bool hasTransparency,
                const VAArray<uint8_t>& data) const;

            Resources::IRenderState* CreateRenderState(
                const RenderStateConfig& config,
                RenderPassHandle passHandle) const;

            Resources::IMaterial* CreateMaterial(
                const std::string& name,
                const MaterialTemplate& templ) const;

            Resources::IShader* CreateShader(
                const std::string& name,
                const ShaderConfig& config,
                const VAArray<uint8_t>& data) const;

            Resources::IMesh* CreateMesh(
                const std::string& name,
                const VAArray<Resources::MeshVertex>& vertices,
                const VAArray<uint32_t>& indices) const;

            Resources::IRenderTarget* CreateRenderTarget(
                const Renderer::RenderTargetConfig& config) const;
            Resources::IRenderTarget* CreateRenderTarget(
                const std::string& name,
                VkImage nativeImage,
                VkFormat format) const;

            Resources::IRenderPass* CreateRenderPass(
                const Renderer::RenderPassConfig& config,
                Renderer::PassPosition passPosition,
                VkFormat swapchainFormat,
                VkFormat depthFormat) const;

        private:
            VkPipelineRasterizationStateCreateInfo CreateRasterizerState(
                const RenderStateConfig& stateConfig) const;
            VkPipelineDepthStencilStateCreateInfo CreateDepthStencilState(
                const RenderStateConfig& stateConfig) const;
            std::pair<VkPipelineColorBlendStateCreateInfo, VkPipelineColorBlendAttachmentState>
            CreateColorBlendState(const RenderStateConfig& stateConfig) const;
            std::pair<VkVertexInputBindingDescription, std::vector<
                          VkVertexInputAttributeDescription>> GetVertexInputDesc(
                const RenderStateConfig& stateConfig) const;

            const std::unique_ptr<VulkanDevice>& m_Device;
            VkAllocationCallbacks* m_Allocator;
        };

        inline std::unique_ptr<VulkanResourceFactory> g_VkResourceFactory;
    } // Platform
} // VoidArchitect
