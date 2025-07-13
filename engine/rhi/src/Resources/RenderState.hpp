//
// Created by Michael Desmedt on 27/05/2025.
//
#pragma once

#include <VoidArchitect/Engine/Common/Uuid.hpp>

#include "Shader.hpp"
#include "RendererTypes.hpp"

namespace VoidArchitect
{
    class RenderStateSystem;
}

namespace VoidArchitect::Platform
{
    class IRenderingHardware;
}

namespace VoidArchitect::Resources
{
    using RenderStateHandle = uint32_t;
    static constexpr RenderStateHandle InvalidRenderStateHandle = std::numeric_limits<
        uint32_t>::max();

    struct RenderStateConfig
    {
        std::string name;

        Renderer::MaterialClass materialClass;
        Renderer::RenderPassType passType;
        Renderer::VertexFormat vertexFormat = Renderer::VertexFormat::Position;

        VAArray<Renderer::ResourceBinding> expectedBindings;

        VAArray<Resources::ShaderHandle> shaders;
        VAArray<Renderer::VertexAttribute> vertexAttributes;
        // TODO InputLayout; -> Which data bindings are used?
        // TODO RenderState -> Allow configuration options like culling, depth testing, etc.

        size_t GetBindingsHash() const;
    };

    class IRenderState
    {
        friend class VoidArchitect::RenderStateSystem;

    public:
        virtual ~IRenderState() = default;

        virtual void Bind(Platform::IRenderingHardware& rhi) = 0;
        [[nodiscard]] std::string GetName() const { return m_Name; }
        [[nodiscard]] UUID GetUUID() const { return m_UUID; };

    protected:
        explicit IRenderState(const std::string& name);

        UUID m_UUID;
        std::string m_Name;
    };

    using RenderStatePtr = std::shared_ptr<IRenderState>;
} // namespace VoidArchitect::Resources
