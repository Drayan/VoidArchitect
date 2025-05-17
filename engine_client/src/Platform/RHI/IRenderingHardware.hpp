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
    };
}
