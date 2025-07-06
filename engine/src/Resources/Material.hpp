//
// Created by Michael Desmedt on 20/05/2025.
//
#pragma once

#include "Core/Math/Mat4.hpp"
#include "Core/Math/Vec4.hpp"
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

    struct MeshNode;

    namespace Resources
    {
        using MeshHandle = Handle<MeshNode>;

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
            uint32_t DebugMode;
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
            GeometryRenderData(const Math::Mat4& model, MaterialHandle material, MeshHandle mesh);

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

            /// @brief Check if textures have changed and require rebinding
            /// @return true if texture resources have changed since last update
            ///
            /// This method is used by binding managers to determine if texture
            /// bindings need to be updated. The base implementation returns true
            /// (conservative approach) but derived class can optimize by tracking
            /// actual changes.
            virtual bool HasResourcesChanged() const { return true; }

            /// @brief Mark current resource state as up-to-date
            ///
            /// Called by binding managers after updating bindings to reset
            /// change detection. The base implementation is a no-op but derived
            /// classes should update their change tracking state.
            virtual void MarkResourcesUpdated()
            {
            }

            virtual void SetDiffuseColor(const Math::Vec4& color) = 0;
            virtual void SetTexture(
                Resources::TextureUse use,
                Resources::TextureHandle texture) = 0;

            /// @brief Get material generation for property change detection
            /// @return Current generation counter
            ///
            /// This tracks change to material properties (like diffuse color) but
            /// not texture changes. Use HasResourcesChanged() for texture changes.
            [[nodiscard]] uint32_t GetGeneration() const { return m_Generation; }

        protected:
            explicit IMaterial(const std::string& name);

            std::string m_Name;
            uint32_t m_Generation = std::numeric_limits<uint32_t>::max();

            Math::Vec4 m_DiffuseColor = Math::Vec4::One();
        };
    } // namespace Resources
} // namespace VoidArchitect
