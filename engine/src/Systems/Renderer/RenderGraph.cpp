//
// Created by Michael Desmedt on 02/06/2025.
//
#include "RenderGraph.hpp"

#include <ranges>

#include "PassRenderers.hpp"
#include "RenderGraphBuilder.hpp"
#include "Core/Logger.hpp"

namespace VoidArchitect::Renderer
{
    const char* RenderPassTypeToString(const RenderPassType type)
    {
        switch (type)
        {
            case RenderPassType::Unknown:
                return "Unknown";
            case RenderPassType::ForwardOpaque:
                return "ForwardOpaque";
            case RenderPassType::ForwardTransparent:
                return "ForwardTransparent";
            case RenderPassType::DepthPrepass:
                return "DepthPrepass";
            case RenderPassType::Shadow:
                return "Shadow";
            case RenderPassType::PostProcess:
                return "PostProcess";
            case RenderPassType::UI:
                return "UI";
            default:
                return "Invalid";
        }
    }

    void RenderGraph::AddPass(const std::string& name, IPassRenderer* passRenderer)
    {
        const PassNode node{
            .name = name,
            .declarationIndex = static_cast<uint32_t>(m_Passes.size()),
            .passRenderer = passRenderer,
            .degrees = 0,
            .successors = {},
        };

        m_Passes.push_back(node);
    }

    void RenderGraph::Setup()
    {
        m_ResourcesMap.clear();

        RenderGraphBuilder builder(this);
        // Collect the passes declarations
        for (auto& pass : m_Passes)
        {
            builder.SetCurrentPass(&pass);
            pass.passRenderer->Setup(builder);
        }
    }

    VAArray<IPassRenderer*> RenderGraph::Compile()
    {
        // Walk the resourceMap to deduce the relationship between passes
        for (const auto& access : m_ResourcesMap | std::views::values)
        {
            for (uint32_t i = 0; i < access.size(); ++i)
            {
                for (uint32_t j = 0; j < access.size(); ++j)
                {
                    if (i == j)
                        continue;

                    auto& p1Access = access[i];
                    auto& p2Access = access[j];
                    // Rule 1 & 2 (RAW & WAR)
                    // NOTE : When P1 Read and P2 Write is handled on iteration j, i
                    if (p1Access.type == ResourceAccessType::Write && p2Access.type ==
                        ResourceAccessType::Read)
                    {
                        AddEdge(p1Access.node, p2Access.node);
                    }

                    // Rule 3 (WAW)
                    if (p1Access.type == ResourceAccessType::Write && p2Access.type ==
                        ResourceAccessType::Write)
                    {
                        // Use the declarationIndex to determine the order
                        if (p1Access.node->declarationIndex < p2Access.node->declarationIndex)
                        {
                            AddEdge(p1Access.node, p2Access.node);
                        }
                        else
                        {
                            AddEdge(p2Access.node, p1Access.node);
                        }
                    }
                }
            }
        }

        // Use the Kahn algorithm to compute the execution order
        std::queue<PassNode*> queue;
        VAArray<IPassRenderer*> sortedPasses;
        for (auto& pass : m_Passes)
        {
            if (pass.degrees == 0)
                queue.push(&pass);
        }

        while (!queue.empty())
        {
            auto pass = queue.front();
            queue.pop();
            sortedPasses.push_back(pass->passRenderer);

            // For each successors
            for (auto& successor : pass->successors)
            {
                successor->degrees--;
                if (successor->degrees == 0)
                    queue.push(successor);
            }
        }

        // Cycle checking
        if (sortedPasses.size() != m_Passes.size())
        {
            VA_ENGINE_ERROR("[RenderGraph] Cycle detected in the render graph.");
            return {};
        }

        return sortedPasses;
    }

    void RenderGraph::AddEdge(PassNode* from, PassNode* to)
    {
        // Don't allow double relation
        if (std::ranges::find(from->successors, to) ==
            from->successors.end())
        {
            from->successors.push_back(to);
            to->degrees++;
        }
    }
} // namespace VoidArchitect::Renderer
// VoidArchitect
