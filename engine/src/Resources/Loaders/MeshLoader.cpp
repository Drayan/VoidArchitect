//
// Created by Michael Desmedt on 15/06/2025.
//
#include "MeshLoader.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Core/Logger.hpp"
#include "Core/Math/Constants.hpp"
#include "Core/Math/Math.hpp"
#include "Resources/MeshData.hpp"
#include "Systems/MaterialSystem.hpp"

namespace VoidArchitect::Resources::Loaders
{
    Math::Mat4 ConvertAssimpMatrix(const aiMatrix4x4& mat)
    {
        return {
            mat.a1,
            mat.b1,
            mat.c1,
            mat.d1,
            mat.a2,
            mat.b2,
            mat.c2,
            mat.d2,
            mat.a3,
            mat.b3,
            mat.c3,
            mat.d3,
            mat.a4,
            mat.b4,
            mat.c4,
            mat.d4
        };
    }

    void LogSuspiciousTransforms(const Math::Mat4& transform, const char* nodeName)
    {
        auto scale = transform.GetScale();
        if (Math::Abs(scale.X() - scale.Y()) > Math::EPSILON || Math::Abs(scale.Y() - scale.Z()) >
            Math::EPSILON)
        {
            VA_ENGINE_TRACE(
                "[MeshLoader] Node '{}' has non-uniform scaling: ({:.3f}, {:.3f}, {:.3f}).",
                nodeName,
                scale.X(),
                scale.Y(),
                scale.Z());
        }
    }

    std::string BuildSubMeshName(
        const aiNode* node,
        const uint32_t meshIndex,
        const uint32_t totalMeshes)
    {
        std::string baseName = node->mName.C_Str();
        if (baseName.empty())
        {
            baseName = "Mesh";
        }

        if (totalMeshes == 1) return baseName;

        return baseName + "_" + std::to_string(meshIndex);
    }

    Resources::TextureUse MapAssimpTextureType(const aiTextureType type)
    {
        switch (type)
        {
            case aiTextureType_DIFFUSE:
                return Resources::TextureUse::Diffuse;
            case aiTextureType_SPECULAR:
                return Resources::TextureUse::Specular;
            case aiTextureType_NORMALS:
                return Resources::TextureUse::Normal;
            default:
                return Resources::TextureUse::Diffuse;
        }
    }

    std::string ExtractTextureName(const std::string& texturePath)
    {
        if (texturePath.empty())
        {
            return "";
        }

        const std::filesystem::path path(texturePath);
        return path.stem().string();
    }

    std::string CreateMaterialName(
        const std::string& meshName,
        const aiMaterial* material,
        const uint32_t materialIndex)
    {
        aiString matName;
        if (material->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS && matName.length > 0)
        {
            return meshName + "_" + std::string(matName.C_Str());
        }

        // Fallback to index-based naming
        return meshName + "_" + std::to_string(materialIndex);
    }

    MaterialTemplate ImportAssimpMaterialTemplate(
        const aiMaterial* material,
        const std::string& materialName)
    {
        MaterialTemplate templateData;
        templateData.name = materialName;
        templateData.renderStateClass = "Opaque"; // Default for now

        // Import diffuse color
        aiColor3D diffuseColor;
        if (material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor) == AI_SUCCESS)
        {
            templateData.diffuseColor = Math::Vec4(
                diffuseColor.r,
                diffuseColor.g,
                diffuseColor.b,
                1.0f);
        }
        else
        {
            templateData.diffuseColor = Math::Vec4::One();
        }

        // Import textures - just extract names, let MaterialSystem handle the loading
        auto importTexture = [&](aiTextureType assimpType, MaterialTemplate::TextureConfig& config)
        {
            if (material->GetTextureCount(assimpType) > 0)
            {
                aiString texPath;
                if (material->GetTexture(assimpType, 0, &texPath) == AI_SUCCESS)
                {
                    std::string texturePath(texPath.C_Str());
                    std::string textureName = ExtractTextureName(texturePath);

                    if (!textureName.empty())
                    {
                        config.use = MapAssimpTextureType(assimpType);
                        config.name = textureName;

                        VA_ENGINE_TRACE(
                            "[MeshLoader] Material '{}' - Found {} texture: '{}'.",
                            materialName,
                            static_cast<int>(assimpType),
                            textureName);
                    }
                }
            }
        };

        // Import all supported texture types
        // TODO: Add other texture types
        importTexture(aiTextureType_DIFFUSE, templateData.diffuseTexture);
        importTexture(aiTextureType_SPECULAR, templateData.specularTexture);
        importTexture(aiTextureType_NORMALS, templateData.normalTexture);

        templateData.resourceBindings = {
            // MaterialUBO
            {Renderer::ResourceBindingType::ConstantBuffer, 0, Resources::ShaderStage::All, {}},
            // DiffuseMap
            {Renderer::ResourceBindingType::Texture2D, 1, Resources::ShaderStage::Pixel, {}},
            // SpecularMap
            {Renderer::ResourceBindingType::Texture2D, 2, Resources::ShaderStage::Pixel, {}},
            // NormalMap
            {Renderer::ResourceBindingType::Texture2D, 3, Resources::ShaderStage::Pixel, {}}
        };

        VA_ENGINE_TRACE("[MeshLoader] Created MaterialTemplate '{}'.", materialName);

        return templateData;
    }

    MaterialHandle ImportAssimpMaterial(
        const aiMesh* mesh,
        const aiScene* scene,
        const std::string& meshName)
    {
        // Check if mesh has a material
        if (mesh->mMaterialIndex >= scene->mNumMaterials)
        {
            VA_ENGINE_WARN(
                "[MeshLoader] Mesh '{}' has invalid material index {}, using default material.",
                meshName,
                mesh->mMaterialIndex);

            return g_MaterialSystem->GetHandleForDefaultMaterial();
        }

        // Get the assimp material
        const auto* assimpMaterial = scene->mMaterials[mesh->mMaterialIndex];
        if (!assimpMaterial)
        {
            VA_ENGINE_WARN(
                "[MeshLoader] Mesh '{}' has null material, using default material.",
                meshName);
            return g_MaterialSystem->GetHandleForDefaultMaterial();
        }

        // Create a unique material name
        auto materialName = CreateMaterialName(meshName, assimpMaterial, mesh->mMaterialIndex);

        // Create MaterialTemplate from assimp
        auto matTemplate = ImportAssimpMaterialTemplate(assimpMaterial, materialName);

        // Register the material template in MaterialSystem
        auto materialHandle = g_MaterialSystem->RegisterTemplate(materialName, matTemplate);
        if (materialHandle == InvalidMaterialHandle)
        {
            VA_ENGINE_ERROR(
                "[MeshLoader] Failed to register material '{}', using default material.",
                materialName);
            return g_MaterialSystem->GetHandleForDefaultMaterial();
        }

        // Ensure the loading.
        materialHandle = g_MaterialSystem->GetHandleFor(materialName);

        VA_ENGINE_TRACE(
            "[MeshLoader] Successfully imported and registered material '{}' with handle {}.",
            materialName,
            materialHandle);

        return materialHandle;
    }

    MeshVertex ProcessVertex(const aiMesh* mesh, uint32_t vertexIndex, const Math::Mat4& transform)
    {
        const auto vertex = mesh->mVertices[vertexIndex];
        const auto normal = mesh->mNormals[vertexIndex];
        const auto UV0 = mesh->mTextureCoords[0][vertexIndex];
        const auto tan = mesh->mTangents[vertexIndex];
        const auto bitan = mesh->mBitangents[vertexIndex];

        MeshVertex v;

        // Transform position
        auto pos = transform * Math::Vec4(vertex.x, vertex.y, vertex.z, 1.0f);
        v.Position = {pos.X(), pos.Y(), pos.Z()};

        // Transform normal
        if (mesh->HasNormals())
        {
            auto norm = transform * Math::Vec4(normal.x, normal.y, normal.z, 0.0f);
            v.Normal = {norm.X(), norm.Y(), norm.Z()};
            v.Normal.Normalize();
        }

        // UV0 (no transformation needed)
        if (mesh->HasTextureCoords(0))
        {
            v.UV0 = {UV0.x, UV0.y};
        }

        // Transform tangent
        if (mesh->HasTangentsAndBitangents())
        {
            auto tangent4 = transform * Math::Vec4(tan.x, tan.y, tan.z, 0.0f);
            auto transTangent = Math::Vec3(tangent4.X(), tangent4.Y(), tangent4.Z()).Normalized();

            auto bitangent4 = transform * Math::Vec4(bitan.x, bitan.y, bitan.z, 0.0f);
            auto transBitangent = Math::Vec3(
                bitangent4.X(),
                bitangent4.Y(),
                bitangent4.Z()).Normalized();

            // Calculate handedness: check if (normal x tangent) points in the same direction as bitangent
            auto calculatedBitangent = Math::Vec3::Cross(transTangent, v.Normal);
            auto handedness = (Math::Vec3::Dot(calculatedBitangent, transBitangent) >= 0.0f)
                ? 1.0f
                : -1.0f;

            v.Tangent = Math::Vec4(transTangent, handedness);
        }

        return v;
    }

    void ProcessFace(const aiFace& face, VAArray<uint32_t>& indices, uint32_t vertexOffset)
    {
        for (uint32_t i = 0; i < face.mNumIndices; ++i)
        {
            indices.push_back(face.mIndices[i]);
        }
    }

    void ProcessMesh(
        const aiMesh* mesh,
        const aiNode* node,
        uint32_t meshIndex,
        const aiScene* scene,
        const std::string& meshName,
        VAArray<MeshVertex>& vertices,
        VAArray<uint32_t>& indices,
        VAArray<Resources::SubMeshDescriptor>& submeshes,
        const Math::Mat4& nodeTransform,
        uint32_t& globalVertexOffset,
        uint32_t& globalIndexOffset)
    {
        // Skip empty meshes
        if (mesh->mNumVertices == 0 || mesh->mNumFaces == 0)
        {
            VA_ENGINE_TRACE(
                "[MeshLoader] Skipping empty mesh '{}' in node '{}'.",
                mesh->mName.C_Str(),
                node->mName.C_Str());

            return;
        }

        auto submeshVertexOffset = globalVertexOffset;
        auto submeshIndexOffset = globalIndexOffset;

        LogSuspiciousTransforms(nodeTransform, node->mName.C_Str());

        // Process vertices with transform
        for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
        {
            auto v = ProcessVertex(mesh, i, nodeTransform);
            vertices.push_back(v);
        }

        // Process indices with offset
        for (uint32_t i = 0; i < mesh->mNumFaces; ++i)
        {
            auto face = mesh->mFaces[i];
            ProcessFace(face, indices, submeshVertexOffset);
        }

        auto materialHandle = ImportAssimpMaterial(mesh, scene, meshName);

        // Create SubMeshDescriptor
        auto submeshName = BuildSubMeshName(node, meshIndex, scene->mNumMeshes);
        submeshes.emplace_back(
            submeshName,
            materialHandle,
            submeshIndexOffset,
            mesh->mNumFaces * 3,
            submeshVertexOffset,
            mesh->mNumVertices);

        // Update global offsets
        globalVertexOffset += mesh->mNumVertices;
        globalIndexOffset += mesh->mNumFaces * 3;

        VA_ENGINE_TRACE(
            "[MeshLoader] Processed submesh '{}' with {} vertices and {} indices.",
            submeshName,
            mesh->mNumVertices,
            mesh->mNumFaces * 3);
    }

    void ProcessNode(
        const aiNode* node,
        const aiScene* scene,
        const std::string& meshName,
        VAArray<MeshVertex>& vertices,
        VAArray<uint32_t>& indices,
        VAArray<Resources::SubMeshDescriptor>& submeshes,
        const aiMatrix4x4& parentTransform,
        uint32_t& globalVertexOffset,
        uint32_t& globalIndexOffset)
    {
        // Calculate cumulative transformation
        auto nodeTransform = parentTransform * node->mTransformation;
        auto transform = ConvertAssimpMatrix(nodeTransform);

        // Process all meshes in this node
        for (uint32_t i = 0; i < node->mNumMeshes; ++i)
        {
            auto mesh = scene->mMeshes[node->mMeshes[i]];
            ProcessMesh(
                mesh,
                node,
                i,
                scene,
                meshName,
                vertices,
                indices,
                submeshes,
                transform,
                globalVertexOffset,
                globalIndexOffset);
        }

        // Recursively process children
        for (uint32_t i = 0; i < node->mNumChildren; ++i)
        {
            auto child = node->mChildren[i];
            ProcessNode(
                child,
                scene,
                meshName,
                vertices,
                indices,
                submeshes,
                nodeTransform,
                globalVertexOffset,
                globalIndexOffset);
        }
    }

    MeshLoader::MeshLoader(const std::string& baseAssetPath)
        : ILoader(baseAssetPath)
    {
    }

    std::shared_ptr<IResourceDefinition> MeshLoader::Load(const std::string& name)
    {
        static constexpr std::string extensions[] = {".gltf", ".fbx", ".obj"};
        std::stringstream ss;
        for (const auto& extension : extensions)
        {
            ss.str("");
            ss << m_BaseAssetPath << name << extension;
            VA_ENGINE_TRACE("Trying to load mesh at path: {}", ss.str());
            if (std::filesystem::exists(ss.str()))
            {
                break;
            }
        }

        Assimp::Importer importer;
        importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, 0.01f);
        const auto scene = importer.ReadFile(
            ss.str(),
            aiProcess_GlobalScale | aiProcess_CalcTangentSpace | aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices);

        if (scene == nullptr)
        {
            VA_ENGINE_ERROR(
                "[MeshLoader] Failed to load mesh '{}' with error: {}.",
                name,
                importer.GetErrorString());
            return nullptr;
        }

        if (!scene->HasMeshes())
        {
            VA_ENGINE_WARN("[MeshLoader] Mesh '{}' has no meshes.", name);
            return nullptr;
        }

        auto meshData = new MeshDataDefinition();
        uint32_t globalVertexOffset = 0;
        uint32_t globalIndexOffset = 0;
        aiMatrix4x4 identityMatrix;

        ProcessNode(
            scene->mRootNode,
            scene,
            name,
            meshData->m_Vertices,
            meshData->m_Indices,
            meshData->m_Submeshes,
            identityMatrix,
            globalVertexOffset,
            globalIndexOffset);

        VA_ENGINE_TRACE(
            "[MeshLoader] Loaded mesh '{}' with {} submeshes, {} total vertices, {} total indices.",
            name,
            meshData->m_Submeshes.size(),
            meshData->m_Vertices.size(),
            meshData->m_Indices.size());

        return MeshDataDefinitionPtr(meshData);
    }
}
