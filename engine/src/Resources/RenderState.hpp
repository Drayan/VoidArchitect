//
// Created by Michael Desmedt on 27/05/2025.
//
#pragma once

#include "Core/Uuid.hpp"
#include "Systems/RenderStateSystem.hpp"

namespace VoidArchitect::Platform
{
    class IRenderingHardware;
}

namespace VoidArchitect::Resources
{
    class IRenderState
    {
        friend class VoidArchitect::RenderStateSystem;

    public:
        virtual ~IRenderState() = default;

        virtual void Bind(Platform::IRenderingHardware& rhi) = 0;
        [[nodiscard]] std::string GetName() const { return m_Name; }
        [[nodiscard]] UUID GetUUID() const { return m_UUID; };

        [[nodiscard]] const RenderStateInputLayout& GetInputLayout() const { return m_InputLayout; }

    protected:
        explicit IRenderState(const std::string& name, const RenderStateInputLayout& inputLayout);

        UUID m_UUID;
        std::string m_Name;
        RenderStateInputLayout m_InputLayout;
    };

    using RenderStatePtr = std::shared_ptr<IRenderState>;
} // namespace VoidArchitect::Resources
