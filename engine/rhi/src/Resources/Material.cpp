//
// Created by Michael Desmedt on 20/05/2025.
//
#include "Material.hpp"

#include <VoidArchitect/Engine/Common/Utils.hpp>

namespace VoidArchitect
{
    size_t MaterialTemplate::GetHash() const
    {
        size_t seed = 0;
        HashCombine(seed, name);
        HashCombine(seed, diffuseColor.X());
        HashCombine(seed, diffuseColor.Y());
        HashCombine(seed, diffuseColor.Z());
        HashCombine(seed, diffuseColor.W());
        HashCombine(seed, GetBindingsHash());
        HashCombine(seed, diffuseTexture.name);
        HashCombine(seed, specularTexture.name);
        HashCombine(seed, renderStateClass);
        return seed;
    }

    size_t MaterialTemplate::GetBindingsHash() const
    {
        size_t seed = 0;

        // Sort the bindings by their binding property
        auto bindings = resourceBindings;
        std::sort(bindings.begin(), bindings.end());
        for (auto& binding : bindings)
        {
            HashCombine(seed, binding.binding);
            HashCombine(seed, binding.type);
            HashCombine(seed, binding.stage);
        }

        return seed;
    }

    namespace Resources
    {
        GeometryRenderData::GeometryRenderData()
            : Model(Math::Mat4::Identity()),
              Material()
        {
        }

        GeometryRenderData::GeometryRenderData(
            const Math::Mat4& model,
            MaterialHandle material,
            const MeshHandle mesh)
            : Model(model),
              Mesh(mesh),
              Material(material)
        {
        }

        IMaterial::IMaterial(const std::string& name)
            : m_Name(name),
              m_Generation(0)
        {
        }
    }
}
