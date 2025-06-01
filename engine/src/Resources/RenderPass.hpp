//
// Created by Michael Desmedt on 02/06/2025.
//
#pragma once

#include "Core/Uuid.hpp"

namespace VoidArchitect
{
    class RenderGraph;

    namespace Platform
    {
        class IRenderingHardware;
    }

    namespace Resources
    {
        class IRenderTarget;
        using RenderTargetPtr = std::shared_ptr<IRenderTarget>;

        class IRenderPass
        {
            friend class VoidArchitect::RenderGraph;

        public:
            virtual ~IRenderPass() = default;

            [[nodiscard]] UUID GetUUID() const { return m_UUID; }
            [[nodiscard]] const std::string& GetName() const { return m_Name; }

            virtual void Begin(Platform::IRenderingHardware& rhi) = 0;
            virtual void End(Platform::IRenderingHardware& rhi) = 0;

        protected:
            IRenderPass(const std::string& name);

            virtual void Release() = 0;

            UUID m_UUID;
            std::string m_Name;
        };

        using RenderPassPtr = std::shared_ptr<IRenderPass>;
    }
}
