//
// Created by Michael Desmedt on 07/06/2025.
//
#include "RenderGraphBuilder.hpp"

#include <VoidArchitect/Engine/Common/Logger.hpp>

namespace VoidArchitect::Renderer
{
    RenderGraphBuilder::RenderGraphBuilder(RenderGraph* renderGraph)
        : m_RenderGraph(renderGraph)
    {
    }

    RenderGraphBuilder& RenderGraphBuilder::WritesTo(const std::string& name)
    {
        if (!m_CurrentPass || !m_RenderGraph)
        {
            VA_ENGINE_ERROR(
                "[RenderGraphBuilder] Attempted to register a WRITE access to a resource without a current pass, or without a render graph.");
            return *this;
        }

        const RenderGraph::ResourceAccessInfo accessInfo{
            .node = m_CurrentPass,
            .type = RenderGraph::ResourceAccessType::Write,
        };
        m_RenderGraph->m_ResourcesMap[name].push_back(accessInfo);

        return *this;
    }

    RenderGraphBuilder& RenderGraphBuilder::ReadsFrom(const std::string& name)
    {
        if (!m_CurrentPass || !m_RenderGraph)
        {
            VA_ENGINE_ERROR(
                "[RenderGraphBuilder] Attempted to register a READ access to a resource without a current pass, or without a render graph.");
            return *this;
        }

        const RenderGraph::ResourceAccessInfo accessInfo{
            .node = m_CurrentPass,
            .type = RenderGraph::ResourceAccessType::Read,
        };
        m_RenderGraph->m_ResourcesMap[name].push_back(accessInfo);

        return *this;
    }

    RenderGraphBuilder& RenderGraphBuilder::ReadsFromColorBuffer()
    {
        return ReadsFrom(WELL_KNOWN_RT_VIEWPORT_COLOR);
    }

    RenderGraphBuilder& RenderGraphBuilder::WritesToColorBuffer()
    {
        return WritesTo(WELL_KNOWN_RT_VIEWPORT_COLOR);
    }

    RenderGraphBuilder& RenderGraphBuilder::ReadsFromDepthBuffer()
    {
        return ReadsFrom(WELL_KNOWN_RT_VIEWPORT_DEPTH);
    }

    RenderGraphBuilder& RenderGraphBuilder::WritesToDepthBuffer()
    {
        return WritesTo(WELL_KNOWN_RT_VIEWPORT_DEPTH);
    }
}
