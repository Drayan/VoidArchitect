//
// Created by Michael Desmedt on 20/05/2025.
//
#pragma once

#include "Core/Uuid.hpp"
#include "Core/Math/Mat4.hpp"
#include "Core/Math/Vec4.hpp"

namespace VoidArchitect::Platform
{
    class IRenderingHardware;
}

namespace VoidArchitect
{
    namespace Resources
    {
        class ITexture;
    }

    //NOTE Vulkan give us a max of 256 bytes on G_UBO
    struct GlobalUniformObject
    {
        Math::Mat4 Projection;
        Math::Mat4 View;
        Math::Mat4 Reserved0;
        Math::Mat4 Reserved1;
    };

    struct LocalUniformObject
    {
        Math::Vec4 DiffuseColor; // 16 bytes
        Math::Vec4 Reserved0; // 16 bytes
        Math::Vec4 Reserved1; // 16 bytes
        Math::Vec4 Reserved2; // 16 bytes
    };

    struct GeometryRenderData
    {
        GeometryRenderData();
        GeometryRenderData(UUID objectId, const Math::Mat4& model);

        UUID ObjectId;
        Math::Mat4 Model;
        std::shared_ptr<Resources::ITexture> Textures[16];
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

        virtual void SetObject(
            Platform::IRenderingHardware& rhi,
            const GeometryRenderData& data) = 0;

    protected:
        GlobalUniformObject m_GlobalUniformObject;
    };
} // VoidArchitect
