//
// Created by Michael Desmedt on 14/05/2025.
//
#pragma once
#include "IRenderingHardware.hpp"

namespace VoidArchitect::Platform
{
    enum class RHI_API_TYPE
    {
        None = 0, // Headless application, e.g., for the server.
        Vulkan, // Vulkan implementation of RHI.
    };

    class IRenderingHardware
    {
    public:
        IRenderingHardware() = default;
        virtual ~IRenderingHardware() = default;

        virtual void Resize(uint32_t width, uint32_t height) = 0;

        virtual bool BeginFrame(float deltaTime) = 0;
        virtual bool EndFrame(float deltaTime) = 0;
    };
}
