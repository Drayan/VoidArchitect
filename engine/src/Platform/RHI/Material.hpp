//
// Created by Michael Desmedt on 20/05/2025.
//
#pragma once
#include "Core/Math/Mat4.hpp"

namespace VoidArchitect::Platform
{
    class IRenderingHardware;
}

namespace VoidArchitect
{
    //NOTE Vulkan give us a max of 256 bytes on G_UBO
    struct GlobalUniformObject
    {
        Math::Mat4 Projection;
        Math::Mat4 View;
        Math::Mat4 Reserved0;
        Math::Mat4 Reserved1;
    };

    class IMaterial
    {
    public:
        virtual ~IMaterial() = default;

        virtual void Use(Platform::IRenderingHardware& rhi) = 0;

        virtual void SetGlobalUniforms(
            Platform::IRenderingHardware& rhi,
            const Math::Mat4& projection,
            const Math::Mat4& view) = 0;

        virtual void SetObjectModelConstant(
            Platform::IRenderingHardware& rhi,
            const Math::Mat4& model) = 0;

    protected:
        GlobalUniformObject m_GlobalUniformObject;
    };
} // VoidArchitect
