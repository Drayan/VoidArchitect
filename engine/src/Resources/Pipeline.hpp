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
    class PipelineSystem;
}

namespace VoidArchitect::Resources
{

    class IPipeline
    {
        friend class VoidArchitect::PipelineSystem;

    public:
        virtual ~IPipeline() = default;

        virtual void Bind(Platform::IRenderingHardware& rhi) = 0;

    private:
        UUID m_UUID;
    };

    using PipelinePtr = std::shared_ptr<IPipeline>;

} // namespace VoidArchitect::Resources
