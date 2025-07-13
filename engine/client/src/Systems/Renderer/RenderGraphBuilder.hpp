//
// Created by Michael Desmedt on 07/06/2025.
//
#pragma once
#include "RenderGraph.hpp"

namespace VoidArchitect
{
    namespace Renderer
    {
        class RenderGraphBuilder
        {
        public:
            explicit RenderGraphBuilder(RenderGraph* renderGraph);

            void SetCurrentPass(RenderGraph::PassNode* pass) { m_CurrentPass = pass; };

            RenderGraphBuilder& ReadsFrom(const std::string& name);
            RenderGraphBuilder& WritesTo(const std::string& name);

            RenderGraphBuilder& ReadsFromColorBuffer();
            RenderGraphBuilder& WritesToColorBuffer();
            RenderGraphBuilder& ReadsFromDepthBuffer();
            RenderGraphBuilder& WritesToDepthBuffer();

        private:
            RenderGraph* m_RenderGraph;
            RenderGraph::PassNode* m_CurrentPass = nullptr;
        };
    } // Renderer
} // VoidArchitect
