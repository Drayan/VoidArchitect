//
// Created by Michael Desmedt on 15/06/2025.
//
#pragma once
#include "Loader.hpp"
#include "ResourceDefinition.hpp"
#include "Resources/MeshData.hpp"
#include "Resources/SubMesh.hpp"

namespace VoidArchitect::Resources::Loaders
{
    class MeshDataDefinition final : public IResourceDefinition
    {
        friend class RawMeshLoader;
        friend class VAMLoader;

    public:
        [[nodiscard]] const VAArray<MeshVertex>& GetVertices() const { return m_Vertices; }
        [[nodiscard]] const VAArray<uint32_t>& GetIndices() const { return m_Indices; }

        [[nodiscard]] const VAArray<Resources::SubMeshDescriptor>& GetSubmeshes() const
        {
            return m_Submeshes;
        }

    private:
        VAArray<MeshVertex> m_Vertices;
        VAArray<uint32_t> m_Indices;
        VAArray<Resources::SubMeshDescriptor> m_Submeshes;
    };

    using MeshDataDefinitionPtr = std::shared_ptr<MeshDataDefinition>;

    class RawMeshLoader final : public ILoader
    {
    public:
        explicit RawMeshLoader(const std::string& baseAssetPath);
        ~RawMeshLoader() override = default;

        std::shared_ptr<IResourceDefinition> Load(const std::string& name) override;
    };
} // Loaders
// Resources
// VoidArchitect
