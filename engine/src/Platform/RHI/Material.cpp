//
// Created by Michael Desmedt on 20/05/2025.
//
#include "Material.hpp"
#include "Resources/Texture.hpp"

namespace VoidArchitect
{
    std::shared_ptr<Resources::Texture2D> IMaterial::s_DefaultDiffuseTexture;

    GeometryRenderData::GeometryRenderData()
        : Model(Math::Mat4::Identity())
    {
    }

    GeometryRenderData::GeometryRenderData(const UUID objectId, const Math::Mat4& model)
        : ObjectId(objectId), Model(model)
    {
    }
} // VoidArchitect
