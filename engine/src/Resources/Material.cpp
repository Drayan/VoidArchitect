//
// Created by Michael Desmedt on 20/05/2025.
//
#include "Material.hpp"

#include "Resources/Texture.hpp"
#include "Systems/MaterialSystem.hpp"

namespace VoidArchitect::Resources
{
    Texture2DPtr IMaterial::s_DefaultDiffuseTexture;

    GeometryRenderData::GeometryRenderData()
        : Model(Math::Mat4::Identity()),
          Material(g_MaterialSystem->GetHandleForDefaultMaterial())
    {
    }

    GeometryRenderData::GeometryRenderData(
        const Math::Mat4& model,
        MaterialHandle material,
        const MeshPtr& mesh)
        : Model(model),
          Mesh(mesh),
          Material(material)
    {
    }

    IMaterial::IMaterial(const std::string& name)
        : m_Name(name), m_Generation(0)
    {
    }
} // namespace VoidArchitect::Resources
