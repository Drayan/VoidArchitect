//
// Created by Michael Desmedt on 20/05/2025.
//
#pragma once

#include <memory>

#include "Mesh.hpp"
#include "RenderState.hpp"
#include "Core/Math/Mat4.hpp"
#include "Core/Math/Vec4.hpp"
#include "Core/Uuid.hpp"
#include "Resources/Texture.hpp"

namespace VoidArchitect::Platform
{
    class IRenderingHardware;
}

namespace VoidArchitect
{
    class MaterialSystem;

    namespace Resources
    {
        class ITexture;
        class Texture2D;
        class IMaterial;

        using MaterialPtr = std::shared_ptr<IMaterial>;

        struct GlobalUniformObject
        {
            Math::Mat4 Projection;
            Math::Mat4 View;
            Math::Mat4 UIProjection;
            Math::Mat4 Reserved0;
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
                const MaterialPtr& material,
                const MeshPtr& mesh);

            Math::Mat4 Model;
            MeshPtr Mesh;
            MaterialPtr Material;
        };

        class IMaterial
        {
            friend class VoidArchitect::MaterialSystem;

        public:
            virtual ~IMaterial() = default;

            virtual void SetModel(
                Platform::IRenderingHardware& rhi,
                const Math::Mat4& model,
                const RenderStatePtr& pipeline) = 0;
            virtual void Bind(
                Platform::IRenderingHardware& rhi,
                const RenderStatePtr& pipeline) = 0;

            static void SetDefaultDiffuseTexture(const Resources::Texture2DPtr& defaultTexture)
            {
                s_DefaultDiffuseTexture = defaultTexture;
            }

            [[nodiscard]] UUID GetUUID() const { return m_UUID; }

            [[nodiscard]] const TexturePtr& GetTexture(size_t index = 0) const
            {
                return m_DiffuseTexture;
            }

            Math::Vec4 GetDiffuseColor() const { return m_DiffuseColor; };
            uint32_t GetGeneration() const { return m_Generation; }

            void SetDiffuseColor(const Math::Vec4& color)
            {
                m_DiffuseColor = color;
                m_Generation++;
            }

            void SetTexture(size_t index, const TexturePtr& texture) { m_DiffuseTexture = texture; }

        protected:
            explicit IMaterial(const std::string& name);

            virtual void InitializeResources(Platform::IRenderingHardware& rhi) = 0;
            virtual void ReleaseResources() = 0;

            static Texture2DPtr s_DefaultDiffuseTexture;
            GlobalUniformObject m_GlobalUniformObject;

            UUID m_UUID = InvalidUUID;
            std::string m_Name;
            uint32_t m_Generation = std::numeric_limits<uint32_t>::max();

            Math::Vec4 m_DiffuseColor = Math::Vec4::One();

            TexturePtr m_DiffuseTexture;
        };
    } // namespace Resources
} // namespace VoidArchitect
