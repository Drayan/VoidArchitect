//
// Created by Michael Desmedt on 20/05/2025.
//
#pragma once

#include <memory>

#include "Core/Math/Mat4.hpp"
#include "Core/Math/Vec4.hpp"
#include "Mesh.hpp"
#include "Resources/Texture.hpp"

namespace VoidArchitect::Platform
{
    class IRenderingHardware;
}

namespace VoidArchitect
{
    class MaterialSystem;
    using MaterialHandle = uint32_t;
    static constexpr MaterialHandle InvalidMaterialHandle = std::numeric_limits<uint32_t>::max();

    namespace Resources
    {
        class ITexture;
        class Texture2D;
        class IMaterial;

        struct GlobalUniformObject
        {
            Math::Mat4 Projection;
            Math::Mat4 View;
            Math::Mat4 UIProjection;
            Math::Vec4 LightDirection;
            Math::Vec4 LightColor;
            Math::Vec4 ViewPosition;
        };

        struct MaterialUniformObject
        {
            Math::Vec4 DiffuseColor; // 16 bytes
            Math::Vec4 Reserved0; // 16 bytes
            Math::Vec4 Reserved1; // 16 bytes
            Math::Vec4 Reserved2; // 16 bytes
        };

        struct GeometryRenderData
        {
            GeometryRenderData();
            GeometryRenderData(
                const Math::Mat4& model,
                MaterialHandle material,
                MeshHandle mesh);

            Math::Mat4 Model;
            MeshHandle Mesh;
            MaterialHandle Material;
        };

        class IMaterial
        {
            friend class VoidArchitect::MaterialSystem;

        public:
            static constexpr size_t MAX_TEXTURES = 4;

            virtual ~IMaterial() = default;

            virtual void SetDiffuseColor(const Math::Vec4& color) = 0;
            virtual void SetTexture(
                Resources::TextureUse use,
                Resources::TextureHandle texture) = 0;

            [[nodiscard]] uint32_t GetGeneration() const { return m_Generation; }

        protected:
            explicit IMaterial(const std::string& name);

            std::string m_Name;
            uint32_t m_Generation = std::numeric_limits<uint32_t>::max();

            Math::Vec4 m_DiffuseColor = Math::Vec4::One();
        };
    } // namespace Resources
} // namespace VoidArchitect
