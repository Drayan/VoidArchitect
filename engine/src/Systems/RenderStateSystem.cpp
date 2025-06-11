//
// Created by Michael Desmedt on 27/05/2025.
//
#include "RenderStateSystem.hpp"

#include "RenderPassSystem.hpp"
#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Renderer/RenderGraph.hpp"
#include "ShaderSystem.hpp"
#include "Renderer/RenderSystem.hpp"

namespace VoidArchitect
{
    RenderStateSystem::RenderStateSystem() { LoadDefaultRenderStates(); }

    RenderStateSystem::~RenderStateSystem()
    {
        for (uint32_t i = 0; i < m_RenderStates.size(); ++i)
        {
            delete m_RenderStates[i].renderStatePtr;
        }
        m_RenderStates.clear();
        m_ConfigMap.clear();
    }

    void RenderStateSystem::RegisterPermutation(const RenderStateConfig& config)
    {
        // First, check the config map if this particular permutation exists or not
        const ConfigLookupKey key{config.materialClass, config.passType, config.vertexFormat};
        if (m_ConfigMap.contains(key))
        {
            VA_ENGINE_WARN(
                "[RenderStateSystem] Permutation '{}' already exists for pass '{}'.",
                config.name,
                Renderer::RenderPassTypeToString(config.passType));
            return;
        }

        // Store the permutation config into the map
        m_ConfigMap[key] = config;
        VA_ENGINE_TRACE(
            "[RenderStateSystem] Permutation '{}' registered for pass '{}'.",
            config.name,
            Renderer::RenderPassTypeToString(config.passType));
    }

    RenderStateHandle RenderStateSystem::GetHandleFor(
        const RenderStateCacheKey& key,
        const RenderPassHandle passHandle)
    {
        // Check if this renderState permutation is already in the cache.
        if (m_RenderStateCache.contains(key))
        {
            // This renderState exists, let's check if it's loaded.
            const auto handle = m_RenderStateCache[key];
            const auto& node = m_RenderStates[handle];
            if (node.state == RenderStateLoadingState::Loaded)
            {
                // Okay, this state exists and is loaded, return its handle.
                return handle;
            }

            // The renderState exist but is unloaded, we need to load it first.
            // LoadRenderState(handle);
            return handle;
        }

        // Check the config map to find a suitable config
        const ConfigLookupKey lookupKey{key.materialClass, key.passType, key.vertexFormat};
        if (!m_ConfigMap.contains(lookupKey))
        {
            VA_ENGINE_ERROR(
                "[RenderStateSystem] No render state permutation found for pass '{}' with MaterialClass '{}'.",
                Renderer::RenderPassTypeToString(key.passType),
                static_cast<uint32_t>(key.materialClass));
            return InvalidRenderStateHandle;
        }

        auto& config = m_ConfigMap[lookupKey];

        // This is the first time the system is asked an handle for this renderState
        const auto renderStatePtr = CreateRenderState(config, passHandle);
        if (!renderStatePtr)
        {
            VA_ENGINE_ERROR(
                "[RenderStateSystem] Failed to create render state for pass '{}' and MaterialClass '{}'.",
                Renderer::RenderPassTypeToString(key.passType),
                static_cast<uint32_t>(key.materialClass));
            return InvalidRenderStateHandle;
        }

        const RenderStateData node = {RenderStateLoadingState::Unloaded, config, renderStatePtr};
        const auto handle = GetFreeRenderStateHandle();
        m_RenderStates[handle] = node;
        m_RenderStateCache[key] = handle;

        return handle;
    }

    Resources::IRenderState* RenderStateSystem::GetPointerFor(const RenderStateHandle handle)
    {
        return m_RenderStates[handle].renderStatePtr;
    }

    const RenderStateConfig& RenderStateSystem::GetConfigFor(const RenderStateHandle handle)
    {
        return m_RenderStates[handle].config;
    }

    Resources::IRenderState* RenderStateSystem::CreateRenderState(
        RenderStateConfig& config,
        const RenderPassHandle passHandle)
    {
        // Add required data to the config
        if (config.vertexAttributes.empty())
        {
            switch (config.vertexFormat)
            {
                case Renderer::VertexFormat::Position:
                {
                    config.vertexAttributes.push_back(
                        Renderer::VertexAttribute{
                            Renderer::AttributeType::Vec3,
                            Renderer::AttributeFormat::Float32
                        });
                }
                break;
                case Renderer::VertexFormat::PositionColor:
                {
                    config.vertexAttributes.push_back(
                        Renderer::VertexAttribute{
                            Renderer::AttributeType::Vec3,
                            Renderer::AttributeFormat::Float32
                        });
                    config.vertexAttributes.push_back(
                        Renderer::VertexAttribute{
                            Renderer::AttributeType::Vec4,
                            Renderer::AttributeFormat::Float32
                        });
                }
                break;
                case Renderer::VertexFormat::PositionNormal:
                {
                    config.vertexAttributes.push_back(
                        Renderer::VertexAttribute{
                            Renderer::AttributeType::Vec3,
                            Renderer::AttributeFormat::Float32
                        });
                    config.vertexAttributes.push_back(
                        Renderer::VertexAttribute{
                            Renderer::AttributeType::Vec3,
                            Renderer::AttributeFormat::Float32
                        });
                }
                break;
                case Renderer::VertexFormat::PositionNormalUV:
                {
                    config.vertexAttributes.push_back(
                        Renderer::VertexAttribute{
                            Renderer::AttributeType::Vec3,
                            Renderer::AttributeFormat::Float32
                        });
                    config.vertexAttributes.push_back(
                        Renderer::VertexAttribute{
                            Renderer::AttributeType::Vec3,
                            Renderer::AttributeFormat::Float32
                        });
                    config.vertexAttributes.push_back(
                        Renderer::VertexAttribute{
                            Renderer::AttributeType::Vec2,
                            Renderer::AttributeFormat::Float32
                        });
                }
                break;
                case Renderer::VertexFormat::PositionNormalUVTangent:
                {
                    config.vertexAttributes.push_back(
                        Renderer::VertexAttribute{
                            Renderer::AttributeType::Vec3,
                            Renderer::AttributeFormat::Float32
                        });
                    config.vertexAttributes.push_back(
                        Renderer::VertexAttribute{
                            Renderer::AttributeType::Vec3,
                            Renderer::AttributeFormat::Float32
                        });
                    config.vertexAttributes.push_back(
                        Renderer::VertexAttribute{
                            Renderer::AttributeType::Vec2,
                            Renderer::AttributeFormat::Float32
                        });
                    config.vertexAttributes.push_back(
                        Renderer::VertexAttribute{
                            Renderer::AttributeType::Vec3,
                            Renderer::AttributeFormat::Float32
                        });
                }
                break;
                case Renderer::VertexFormat::PositionUV:
                {
                    config.vertexAttributes.push_back(
                        Renderer::VertexAttribute{
                            Renderer::AttributeType::Vec3,
                            Renderer::AttributeFormat::Float32
                        });
                    config.vertexAttributes.push_back(
                        Renderer::VertexAttribute{
                            Renderer::AttributeType::Vec2,
                            Renderer::AttributeFormat::Float32
                        });
                }
                break;

                // TODO Implement custom vertex format definition (from config file ?)
                case Renderer::VertexFormat::Custom: default: VA_ENGINE_WARN(
                        "[RenderStateSystem] Unknown vertex format for render state '{}'.",
                        config.name);
                    break;
            }
        }

        // Create a new render state resource
        const auto renderState = Renderer::g_RenderSystem->GetRHI()->CreateRenderState(
            config,
            passHandle);

        if (!renderState)
        {
            VA_ENGINE_WARN(
                "[RenderStateSystem] Failed to create render state '{}' for pass '{}'.",
                config.name,
                Renderer::RenderPassTypeToString(config.passType));
            return nullptr;
        }

        VA_ENGINE_TRACE(
            "[RenderStateSystem] RenderState '{}' created for pass '{}'.",
            config.name,
            Renderer::RenderPassTypeToString(config.passType));

        return renderState;
    }

    void RenderStateSystem::LoadDefaultRenderStates()
    {
        auto renderStateConfig = RenderStateConfig{};
        renderStateConfig.name = "Default";

        renderStateConfig.materialClass = Renderer::MaterialClass::Standard;
        renderStateConfig.passType = Renderer::RenderPassType::ForwardOpaque;
        renderStateConfig.vertexFormat = Renderer::VertexFormat::PositionNormalUV;

        // Try to load the default shaders into the render state.
        auto vertexShader = g_ShaderSystem->GetHandleFor("BuiltinObject.vert");
        auto pixelShader = g_ShaderSystem->GetHandleFor("BuiltinObject.pixl");

        renderStateConfig.shaders.emplace_back(vertexShader);
        renderStateConfig.shaders.emplace_back(pixelShader);

        renderStateConfig.expectedBindings = {
            Renderer::ResourceBinding{
                Renderer::ResourceBindingType::ConstantBuffer,
                0,
                Resources::ShaderStage::All
            },
            Renderer::ResourceBinding{
                Renderer::ResourceBindingType::Texture2D,
                1,
                Resources::ShaderStage::Pixel
            },
            Renderer::ResourceBinding{
                Renderer::ResourceBindingType::Texture2D,
                2,
                Resources::ShaderStage::Pixel
            }
        };

        RegisterPermutation(renderStateConfig);
        VA_ENGINE_INFO("[RenderStateSystem] Default render state template registered.");

        // UI RenderState
        auto uiRenderStateConfig = RenderStateConfig{};
        uiRenderStateConfig.name = "UI";

        uiRenderStateConfig.materialClass = Renderer::MaterialClass::UI;
        uiRenderStateConfig.passType = Renderer::RenderPassType::UI;
        uiRenderStateConfig.vertexFormat = Renderer::VertexFormat::PositionNormalUV;

        // Try to load the default UI shaders into the render state.
        auto uiVertexShader = g_ShaderSystem->GetHandleFor("UI.vert");
        auto uiPixelShader = g_ShaderSystem->GetHandleFor("UI.pixl");

        uiRenderStateConfig.shaders.emplace_back(uiVertexShader);
        uiRenderStateConfig.shaders.emplace_back(uiPixelShader);

        uiRenderStateConfig.expectedBindings = {
            Renderer::ResourceBinding{
                Renderer::ResourceBindingType::ConstantBuffer,
                0,
                Resources::ShaderStage::All
            },
            Renderer::ResourceBinding{
                Renderer::ResourceBindingType::Texture2D,
                1,
                Resources::ShaderStage::Pixel
            },
        };

        RegisterPermutation(uiRenderStateConfig);
        VA_ENGINE_INFO("[RenderStateSystem] UI render state template registered.");
    }

    RenderStateHandle RenderStateSystem::GetFreeRenderStateHandle()
    {
        // If we have a free handle in the queue, return that first.
        if (!m_FreeRenderStateHandles.empty())
        {
            const auto handle = m_FreeRenderStateHandles.front();
            m_FreeRenderStateHandles.pop();
            return handle;
        }

        // Otherwise, return the next handle and increment it.
        if (m_NextFreeRenderStateHandle >= m_RenderStates.size())
        {
            m_RenderStates.resize(m_NextFreeRenderStateHandle + 1);
        }
        const auto handle = m_NextFreeRenderStateHandle;
        m_NextFreeRenderStateHandle++;

        return handle;
    }

    size_t RenderStateCacheKey::GetHash() const
    {
        size_t seed = 0;

        HashCombine(seed, static_cast<int32_t>(materialClass));
        HashCombine(seed, static_cast<int32_t>(passType));
        HashCombine(seed, static_cast<int32_t>(vertexFormat));

        HashCombine(seed, passSignature.GetHash());

        return seed;
    }

    size_t RenderStateConfig::GetBindingsHash() const
    {
        size_t seed = 0;

        // Sort the bindings by their binding property
        auto bindings = expectedBindings;
        std::sort(bindings.begin(), bindings.end());
        for (auto& binding : bindings)
        {
            HashCombine(seed, binding.binding);
            HashCombine(seed, binding.type);
            HashCombine(seed, binding.stage);
        }

        return seed;
    }

    bool RenderStateCacheKey::operator==(const RenderStateCacheKey& other) const
    {
        return materialClass == other.materialClass && passSignature == other.passSignature &&
            passType == other.passType && vertexFormat == other.vertexFormat;
    }
} // namespace VoidArchitect
