//
// Created by Michael Desmedt on 02/06/2025.
//
#pragma once
#include "Resources/Material.hpp"
#include "Systems/RenderPassSystem.hpp"

namespace VoidArchitect
{
    namespace Platform
    {
        class IRenderingHardware;
    }

    namespace Resources
    {
        class Texture2D;
        using Texture2DPtr = std::shared_ptr<Texture2D>;
    } // namespace Resources

    namespace Renderer
    {
        class IPassRenderer;
        enum class TextureFormat;
        enum class RenderPassType;

        const char* RenderPassTypeToString(RenderPassType type);

        class RenderGraph
        {
            friend class RenderGraphBuilder;

        public:
            RenderGraph() = default;
            ~RenderGraph() = default;

            void AddPass(const std::string& name, IPassRenderer* passRenderer);
            void ImportRenderTarget(const std::string& name, Resources::RenderTargetHandle handle);

            void Setup();
            RenderGraphExecutionPlan Compile();

        private:
            struct PassNode
            {
                std::string name;
                uint32_t declarationIndex;
                IPassRenderer* passRenderer;

                uint32_t degrees;
                VAArray<PassNode*> successors;
            };

            enum class ResourceAccessType
            {
                Read,
                Write
            };

            struct ResourceAccessInfo
            {
                PassNode* node;
                ResourceAccessType type;
            };

            static void AddEdge(PassNode* from, PassNode* to);

            VAArray<PassNode> m_Passes;
            VAHashMap<std::string, VAArray<ResourceAccessInfo>> m_ResourcesMap;
            VAHashMap<std::string, Resources::RenderTargetHandle> m_RenderTargets;
        };
    } // namespace Renderer
} // namespace VoidArchitect
