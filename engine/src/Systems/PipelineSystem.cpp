//
// Created by Michael Desmedt on 27/05/2025.
//
#include "PipelineSystem.hpp"

#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Renderer/RenderCommand.hpp"
#include "ShaderSystem.hpp"

namespace VoidArchitect
{
    PipelineSystem::PipelineSystem()
    {
        m_PipelineCache.reserve(128);
        GenerateDefaultPipelines();
    }

    Resources::PipelinePtr PipelineSystem::CreatePipeline(PipelineConfig& config)
    {
        // Add required data to the config
        switch (config.vertexFormat)
        {
            case VertexFormat::Position:
            {
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec3, AttributeFormat::Float32});
            }
            break;
            case VertexFormat::PositionColor:
            {
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec3, AttributeFormat::Float32});
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec4, AttributeFormat::Float32});
            }
            break;
            case VertexFormat::PositionNormal:
            {
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec3, AttributeFormat::Float32});
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec3, AttributeFormat::Float32});
            }
            break;
            case VertexFormat::PositionNormalUV:
            {
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec3, AttributeFormat::Float32});
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec3, AttributeFormat::Float32});
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec2, AttributeFormat::Float32});
            }
            break;
            case VertexFormat::PositionNormalUVTangent:
            {
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec3, AttributeFormat::Float32});
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec3, AttributeFormat::Float32});
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec2, AttributeFormat::Float32});
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec3, AttributeFormat::Float32});
            }
            break;
            case VertexFormat::PositionUV:
            {
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec3, AttributeFormat::Float32});
                config.vertexAttributes.push_back(
                    VertexAttribute{VertexAttributeType::Vec2, AttributeFormat::Float32});
            }
            break;

                // TODO Implement custom vertex format definition (from config file ?)
            case VertexFormat::Custom:
            default:
                VA_ENGINE_WARN(
                    "[PipelineSystem] Unknown vertex format for pipeline '{}'.", config.name);
                break;
        }

        // Create a new pipeline resource
        const auto pipeline = Renderer::RenderCommand::GetRHIRef().CreatePipeline(config);

        if (!pipeline)
        {
            VA_ENGINE_WARN("[PipelineSystem] Failed to create pipeline '{}'.", config.name);
            return nullptr;
        }

        // Store the pipeline in the cache
        auto pipelinePtr = Resources::PipelinePtr(pipeline, PipelineDeleter{this});
        m_PipelineCache[pipeline->m_UUID] = pipelinePtr;

        VA_ENGINE_TRACE("[PipelineSystem] Pipeline '{}' created.", config.name);

        return pipelinePtr;
    }

    void PipelineSystem::GenerateDefaultPipelines()
    {
        auto pipelineConfig = PipelineConfig{};
        pipelineConfig.name = "Default";

        // Try to load the default shaders into the pipeline.
        auto vertexShader = g_ShaderSystem->LoadShader("BuiltinObject.vert");
        auto pixelShader = g_ShaderSystem->LoadShader("BuiltinObject.pixl");

        pipelineConfig.shaders.emplace_back(vertexShader);
        pipelineConfig.shaders.emplace_back(pixelShader);

        pipelineConfig.vertexFormat = VertexFormat::PositionUV;
        pipelineConfig.inputLayout = PipelineInputLayout{}; // Use default configuration
        m_DefaultPipeline = CreatePipeline(pipelineConfig);
    }

    void PipelineSystem::ReleasePipeline(const Resources::IPipeline* pipeline)
    {
        // Remove from the cache
        if (const auto it = m_PipelineCache.find(pipeline->m_UUID); it != m_PipelineCache.end())
        {
            VA_ENGINE_TRACE("[PipelineSystem] Releasing pipeline '{}'.", "I should get a name");
            m_PipelineCache.erase(it);
        }
    }

    void PipelineSystem::PipelineDeleter::operator()(const Resources::IPipeline* pipeline) const
    {
        system->ReleasePipeline(pipeline);
        delete pipeline;
    }
} // namespace VoidArchitect
