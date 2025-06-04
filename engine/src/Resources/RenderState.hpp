//
// Created by Michael Desmedt on 27/05/2025.
//
#pragma once

#include "Core/Uuid.hpp"

namespace VoidArchitect::Platform
{
    class IRenderingHardware;
}

namespace VoidArchitect
{
    class RenderStateSystem;
}

namespace VoidArchitect::Resources
{
    class IRenderState
    {
        friend class VoidArchitect::RenderStateSystem;

    public:
        virtual ~IRenderState() = default;

        virtual void Bind(Platform::IRenderingHardware& rhi) = 0;
        [[nodiscard]] std::string GetName() const { return m_Name; };

    protected:
        explicit IRenderState(const std::string& name);

        UUID m_UUID;
        std::string m_Name;
    };

    using RenderStatePtr = std::shared_ptr<IRenderState>;
} // namespace VoidArchitect::Resources
