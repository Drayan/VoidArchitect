//
// Created by Michael Desmedt on 08/06/2025.
//
#pragma once
#include "VulkanDevice.hpp"

#include <Resources/SubMesh.hpp>
#include <Resources/RenderPass.hpp>

namespace VoidArchitect
{
    namespace Renderer
    {
        struct RenderTargetConfig;
    }

    namespace Resources
    {
        struct ShaderConfig;
        struct RenderStateConfig;
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

            [[nodiscard]] Resources::Texture2D* CreateTexture2D(
                const std::string& name,
                uint32_t width,
                uint32_t height,
                uint8_t channels,
                bool hasTransparency,
                const VAArray<uint8_t>& data) const;

            [[nodiscard]] Resources::IRenderState* CreateRenderState(
                const Resources::RenderStateConfig& config,
                Resources::RenderPassHandle passHandle) const;

            [[nodiscard]] Resources::IMaterial* CreateMaterial(
                const std::string& name,
                const MaterialTemplate& templ) const;

            [[nodiscard]] Resources::IShader* CreateShader(
                const std::string& name,
                const Resources::ShaderConfig& config,
                const VAArray<uint8_t>& data) const;

            [[nodiscard]] Resources::IMesh* CreateMesh(
                const std::string& name,
                const std::shared_ptr<Resources::MeshData>& data,
                const VAArray<Resources::SubMeshDescriptor>& submeshes) const;

            [[nodiscard]] Resources::IRenderTarget* CreateRenderTarget(
                const Renderer::RenderTargetConfig& config) const;
            Resources::IRenderTarget* CreateRenderTarget(
                const std::string& name,
                VkImage nativeImage,
                VkFormat format) const;

            [[nodiscard]] Resources::IRenderPass* CreateRenderPass(
                const Renderer::RenderPassConfig& config,
                Renderer::PassPosition passPosition,
                VkFormat swapchainFormat,
                VkFormat depthFormat) const;

        private:
            [[nodiscard]] VkPipelineRasterizationStateCreateInfo CreateRasterizerState(
                const Resources::RenderStateConfig& stateConfig) const;
            [[nodiscard]] VkPipelineDepthStencilStateCreateInfo CreateDepthStencilState(
                const Resources::RenderStateConfig& stateConfig) const;
            [[nodiscard]] std::pair<VkPipelineColorBlendStateCreateInfo,
                                    VkPipelineColorBlendAttachmentState> CreateColorBlendState(
                const Resources::RenderStateConfig& stateConfig) const;
            [[nodiscard]] std::pair<VkVertexInputBindingDescription, std::vector<
                                        VkVertexInputAttributeDescription>> GetVertexInputDesc(
                const Resources::RenderStateConfig& stateConfig) const;

            const std::unique_ptr<VulkanDevice>& m_Device;
            VkAllocationCallbacks* m_Allocator;
        };

        inline std::unique_ptr<VulkanResourceFactory> g_VkResourceFactory;
    } // Platform
} // VoidArchitect
