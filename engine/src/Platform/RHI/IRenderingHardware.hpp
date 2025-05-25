//
// Created by Michael Desmedt on 14/05/2025.
//
#pragma once
#include "IRenderingHardware.hpp"

namespace VoidArchitect
{
    struct GeometryRenderData;
}

namespace VoidArchitect::Math
{
    class Mat4;
}

namespace VoidArchitect::Resources
{
    class Texture2D;
}

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
        virtual void WaitIdle(uint64_t timeout) = 0;

        virtual bool BeginFrame(float deltaTime) = 0;
        virtual bool EndFrame(float deltaTime) = 0;

        virtual void UpdateGlobalState(const Math::Mat4& projection, const Math::Mat4& view) = 0;
        virtual void UpdateObjectState(const GeometryRenderData& data) = 0;

        ///////////////////////////////////////////////////////////////////////
        //// Resources ////////////////////////////////////////////////////////
        ///////////////////////////////////////////////////////////////////////
        virtual Resources::Texture2D* CreateTexture2D(
            const std::string& name,
            uint32_t width,
            uint32_t height,
            uint8_t channels,
            bool hasTransparency,
            const std::vector<uint8_t>& data) = 0;
    };
}
