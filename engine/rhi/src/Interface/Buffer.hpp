//
// Created by Michael Desmedt on 19/05/2025.
//
#pragma once

namespace VoidArchitect
{
    namespace Platform
    {
        class IRenderingHardware;
    }

    class IBuffer
    {
    public:
        virtual ~IBuffer() = default;

        virtual void Bind(Platform::IRenderingHardware& rhi) = 0;
        virtual void Unbind() = 0;
    };
} // namespace VoidArchitect
