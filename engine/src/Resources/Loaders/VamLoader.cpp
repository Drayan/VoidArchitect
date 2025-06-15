//
// Created by Michael Desmedt on 15/06/2025.
//
#include "VamLoader.hpp"

#include "Core/Logger.hpp"
#include "Systems/MaterialSystem.hpp"

namespace VoidArchitect::Resources::Loaders
{
    VAMLoader::VAMLoader(const std::string& baseAssetPath)
        : ILoader(baseAssetPath),
          m_CompressionSettings(VAMCompressionSettings::Default())
    {
        // Create raw loader for fallback
        m_RawMeshLoader = std::make_unique<RawMeshLoader>(baseAssetPath);

        // Ensure cache directory exists
        auto cacheDir = m_BaseAssetPath + "../cache/";
        if (!std::filesystem::exists(cacheDir))
        {
            std::filesystem::create_directory(cacheDir);
            VA_ENGINE_INFO("[VAMLoader] Created cache directory: {}", cacheDir);
        }

        // Log compression status
        if (VAMCompression::IsLZ4Available())
        {
            VA_ENGINE_INFO("[VAMLoader] LZ4 compression is available and enabled.");
        }
        else
        {
            VA_ENGINE_INFO(
                "[VAMLoader] LZ4 compression not available, files will be stored uncompressed.");
            m_CompressionSettings.enableCompression = false;
        }
    }

    std::shared_ptr<IResourceDefinition> VAMLoader::Load(const std::string& name)
    {
        const auto vamPath = GetVAMPath(name);

        // Check if VAM cache is valid
        if (const auto sourcePath = FindSourceAsset(name); IsVAMValid(vamPath, sourcePath))
        {
            VA_ENGINE_TRACE("[VAMLoader] Loading cached VAM: {}", name);
            if (auto meshData = LoadMeshFromVAM(vamPath))
            {
                return meshData;
            }

            VA_ENGINE_WARN(
                "[VAMLoader] Failed to load cached VAM, falling back to import: {}",
                name);
        }

        // Cache miss or invalid - import and bake
        VA_ENGINE_TRACE("[VAMLoader] VAM cache miss for '{}’, importing and baking...", name);
        return ImportAndBake(name);
    }

    bool VAMLoader::SaveMeshToVAM(
        const std::string& vamPath,
        const std::string& sourcePath,
        const MeshDataDefinition& meshData,
        const VAMCompressionSettings& compressionSettings)
    {
        try
        {
            std::ofstream file(vamPath, std::ios::binary);
            if (!file.is_open())
            {
                VA_ENGINE_ERROR("[VAMLoader] Failed to open VAM file for writing: {}", vamPath);
                return false;
            }

            // Prepare string table and material data
            VAArray<uint8_t> stringTable;
            VAHashMap<std::string, uint32_t> stringOffsets;
            VAArray<VAMResourceBinding> allBindings;

            for (const auto& submesh : meshData.GetSubmeshes())
                AddToStringTable(submesh.name, stringTable, stringOffsets);

            // Convert materials
            auto vamMaterials = ConvertMaterialsToVAM(
                meshData.GetSubmeshes(),
                stringTable,
                stringOffsets,
                allBindings);

            // Prepare all data sections for potential compression
            VAArray<uint8_t> allData;

            // Calculate original section sizes
            auto stringTableSize = static_cast<uint32_t>(stringTable.size());
            auto verticesSize = static_cast<uint32_t>(meshData.GetVertices().size()) * sizeof(
                VAMVertex);
            auto indicesSize = static_cast<uint32_t>(meshData.GetIndices().size()) * sizeof(
                uint32_t);
            auto submeshesSize = static_cast<uint32_t>(meshData.GetSubmeshes().size()) * sizeof(
                VAMSubMeshDescriptor);
            auto materialsSize = static_cast<uint32_t>(vamMaterials.size()) * sizeof(
                VAMMAterialTemplate);
            auto bindingsSize = static_cast<uint32_t>(allBindings.size()) * sizeof(
                VAMResourceBinding);

            auto totalDataSize = stringTableSize + verticesSize + indicesSize + submeshesSize +
                materialsSize + bindingsSize;

            // Decide whether to compress
            bool shouldCompress = compressionSettings.enableCompression &&
                VAMCompression::IsLZ4Available() && totalDataSize >= compressionSettings.
                minSizeThreshold;

            // Calculate section offsets
            VAMHeader header{};
            memcpy(header.magic, VAM_MAGIC, sizeof(header.magic));
            header.version = VAM_VERSION;
            header.flags = shouldCompress
                ? static_cast<uint32_t>(VAMFlags::Compressed)
                : static_cast<uint32_t>(VAMFlags::None);

            // Get source timestamp (if available)
            if (!sourcePath.empty() && std::filesystem::exists(sourcePath))
            {
                try
                {
                    auto sourceTime = std::filesystem::last_write_time(sourcePath);

                    auto duration = sourceTime.time_since_epoch();
                    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
                    header.sourceTimestamp = static_cast<uint64_t>(seconds.count());

                    VA_ENGINE_TRACE(
                        "[VAMLoader] Stored source timestamp: {} for file: {}",
                        header.sourceTimestamp,
                        sourcePath);
                }
                catch (const std::exception& ex)
                {
                    VA_ENGINE_WARN("[VAMLoader] Failed to get source timestamp: {}.", ex.what());
                    header.sourceTimestamp = 0;
                }
            }
            else
            {
                VA_ENGINE_WARN("[VAMLoader] Source file not found for timestamp: {}", sourcePath);
                header.sourceTimestamp = 0;
            }

            // Set counts
            header.vertexCount = static_cast<uint32_t>(meshData.GetVertices().size());
            header.indexCount = static_cast<uint32_t>(meshData.GetIndices().size());
            header.submeshCount = static_cast<uint32_t>(meshData.GetSubmeshes().size());
            header.materialCount = static_cast<uint32_t>(vamMaterials.size());

            if (shouldCompress)
            {
                // Assemble all data into a single buffer for compression
                allData.reserve(totalDataSize);

                // Add string table
                allData.insert(allData.end(), stringTable.begin(), stringTable.end());

                // Write vertices (convert to VAM format)
                for (const auto& vertex : meshData.GetVertices())
                {
                    VAMVertex vamVertex(vertex.Position, vertex.Normal, vertex.UV0, vertex.Tangent);
                    auto* vertexBytes = reinterpret_cast<const uint8_t*>(&vamVertex);
                    allData.insert(allData.end(), vertexBytes, vertexBytes + sizeof(VAMVertex));
                }

                // Write indices
                if (!meshData.GetIndices().empty())
                {
                    auto* indicesBytes = reinterpret_cast<const uint8_t*>(meshData.GetIndices().
                        data());
                    allData.insert(allData.end(), indicesBytes, indicesBytes + indicesSize);
                }

                // Write submeshes (convert material handles to indices)
                VAArray<VAMSubMeshDescriptor> vamSubmeshes;
                VAHashMap<MaterialHandle, uint32_t> materialIndexMap;
                uint32_t matIndex = 0;
                for (const auto& submesh : meshData.GetSubmeshes())
                {
                    if (!materialIndexMap.contains(submesh.material))
                    {
                        materialIndexMap[submesh.material] = matIndex++;
                    }
                }

                for (const auto& submesh : meshData.GetSubmeshes())
                {
                    VAMSubMeshDescriptor vamSubmesh{};
                    auto it = stringOffsets.find(submesh.name);
                    if (it != stringOffsets.end())
                    {
                        vamSubmesh.nameOffset = it->second;
                    }
                    else
                    {
                        VA_ENGINE_ERROR(
                            "[VAMLoader] Submesh name '{}' not found in string table.",
                            submesh.name);
                        vamSubmesh.nameOffset = 0;
                    }
                    vamSubmesh.materialIndex = materialIndexMap[submesh.material];
                    vamSubmesh.indexOffset = submesh.indexOffset;
                    vamSubmesh.indexCount = submesh.indexCount;
                    vamSubmesh.vertexOffset = submesh.vertexOffset;
                    vamSubmesh.vertexCount = submesh.vertexCount;
                    vamSubmesh.reserved = 0;

                    vamSubmeshes.push_back(vamSubmesh);
                }

                // Add submeshes to data
                if (!vamSubmeshes.empty())
                {
                    auto* submeshesBytes = reinterpret_cast<const uint8_t*>(vamSubmeshes.data());
                    allData.insert(allData.end(), submeshesBytes, submeshesBytes + submeshesSize);
                }

                // Write materials
                if (!vamMaterials.empty())
                {
                    auto* materialsBytes = reinterpret_cast<const uint8_t*>(vamMaterials.data());
                    allData.insert(allData.end(), materialsBytes, materialsBytes + materialsSize);
                }

                // Write resource bindings
                if (!allBindings.empty())
                {
                    auto* bindingsBytes = reinterpret_cast<const uint8_t*>(allBindings.data());
                    allData.insert(allData.end(), bindingsBytes, bindingsBytes + bindingsSize);
                }

                // Compress all data
                auto compressionResult = VAMCompression::Compress(allData);
                if (compressionResult.success)
                {
                    auto compressionRatio = VAMCompression::GetCompressionRatio(
                        compressionResult.originalSize,
                        compressionResult.compressedSize);

                    // Check if compression worth it
                    if (compressionRatio >= compressionSettings.minCompressionRatio)
                    {
                        // Use compressed data
                        header.uncompressedSize = compressionResult.originalSize;
                        header.SetCompressionRatio(compressionRatio);
                        header.stringTableSize = compressionResult.compressedSize;

                        // Store original section sizes for parsing
                        header.originalStringTableSize = stringTableSize;
                        header.originalVerticesSize = verticesSize;
                        header.originalIndicesSize = indicesSize;
                        header.originalSubmeshesSize = submeshesSize;
                        header.originalMaterialsSize = materialsSize;
                        header.originalBindingsSize = bindingsSize;

                        // Set offsets for compressed format (everything is in one comrpessed block)
                        header.stringTableOffset = sizeof(VAMHeader);
                        header.verticesOffset = 0;
                        header.indicesOffset = 0;
                        header.submeshesOffset = 0;
                        header.materialsOffset = 0;

                        // Write header
                        file.write(reinterpret_cast<const char*>(&header), sizeof(header));

                        // Write compressed data
                        file.write(
                            reinterpret_cast<const char*>(compressionResult.compressedData.data()),
                            compressionResult.compressedSize);

                        file.close();

                        VA_ENGINE_TRACE(
                            "[VAMLoader] Successfully saved compressed VAM: {} ({} vertices, {} indices, {} submeshes, {} materials) [{} -> {} bytes, {:.1f}% savings]",
                            vamPath,
                            header.vertexCount,
                            header.indexCount,
                            header.submeshCount,
                            header.materialCount,
                            compressionResult.originalSize,
                            compressionResult.compressedSize,
                            compressionRatio * 100.0f);

                        return true;
                    }
                    else
                    {
                        VA_ENGINE_TRACE(
                            "[VAMLoader] Compression ratio too low ({:.1f}%), storing uncompressed.",
                            compressionRatio * 100.0f);
                    }
                }
                else
                {
                    VA_ENGINE_WARN("[VAMLoader] Compression failed, storing uncompressed.");
                }
            }

            // Store uncompressed (either by choice or because compression failed/wasn't worth it)
            header.flags = static_cast<uint32_t>(VAMFlags::None);
            header.uncompressedSize = 0;
            header.compressionRatio = 0;
            header.stringTableSize = stringTableSize;

            // Clear original size fields (not used in uncompressed format)
            header.originalStringTableSize = 0;
            header.originalVerticesSize = 0;
            header.originalIndicesSize = 0;
            header.originalSubmeshesSize = 0;
            header.originalMaterialsSize = 0;
            header.originalBindingsSize = 0;

            // Calculate section offsets for uncompressed format
            uint32_t currentOffset = sizeof(VAMHeader);

            header.stringTableOffset = currentOffset;
            currentOffset += stringTableSize;

            header.verticesOffset = currentOffset;
            currentOffset += verticesSize;

            header.indicesOffset = currentOffset;
            currentOffset += indicesSize;

            header.submeshesOffset = currentOffset;
            currentOffset += submeshesSize;

            header.materialsOffset = currentOffset;

            // Write header
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));

            // Write section uncompressed

            // Write string table
            if (!stringTable.empty())
            {
                file.write(reinterpret_cast<const char*>(stringTable.data()), stringTable.size());
            }

            // Write vertices
            for (const auto& vertex : meshData.GetVertices())
            {
                VAMVertex vamVertex(vertex.Position, vertex.Normal, vertex.UV0, vertex.Tangent);
                file.write(reinterpret_cast<const char*>(&vamVertex), sizeof(vamVertex));
            }

            // Write indices
            if (!meshData.GetIndices().empty())
            {
                file.write(
                    reinterpret_cast<const char*>(meshData.GetIndices().data()),
                    meshData.GetIndices().size() * sizeof(uint32_t));
            }

            // Write submeshes
            VAHashMap<MaterialHandle, uint32_t> materialIndexMap;
            uint32_t matIndex = 0;
            for (const auto& submesh : meshData.GetSubmeshes())
            {
                if (materialIndexMap.contains(submesh.material))
                {
                    materialIndexMap[submesh.material] = matIndex++;
                }
            }

            for (const auto& submesh : meshData.GetSubmeshes())
            {
                VAMSubMeshDescriptor vamSubmesh{};
                auto it = stringOffsets.find(submesh.name);
                if (it != stringOffsets.end())
                {
                    vamSubmesh.nameOffset = it->second;
                }
                else
                {
                    VA_ENGINE_ERROR(
                        "[VAMLoader] Submesh name '{}' not found in string table.",
                        submesh.name);
                    vamSubmesh.nameOffset = 0;
                }

                vamSubmesh.materialIndex = materialIndexMap[submesh.material];
                vamSubmesh.indexOffset = submesh.indexOffset;
                vamSubmesh.indexCount = submesh.indexCount;
                vamSubmesh.vertexOffset = submesh.vertexOffset;
                vamSubmesh.vertexCount = submesh.vertexCount;
                vamSubmesh.reserved = 0;

                file.write(reinterpret_cast<const char*>(&vamSubmesh), sizeof(vamSubmesh));
            }

            // Write materials
            for (const auto& vamMat : vamMaterials)
            {
                file.write(reinterpret_cast<const char*>(&vamMat), sizeof(vamMat));
            }

            // Write resource bindings
            if (!allBindings.empty())
            {
                file.write(
                    reinterpret_cast<const char*>(allBindings.data()),
                    allBindings.size() * sizeof(VAMResourceBinding));
            }

            file.close();

            VA_ENGINE_TRACE(
                "[VAMLoader] Successfully saved VAM: {} ({} vertices, {} indices, {} submeshes, {} materials) [{} bytes]",
                vamPath,
                header.vertexCount,
                header.indexCount,
                header.submeshCount,
                header.materialCount,
                totalDataSize);

            return true;
        }
        catch (const std::exception& ex)
        {
            VA_ENGINE_ERROR("[VAMLoader] Failed to save VAM: {} : {}", vamPath, ex.what());
            return false;
        }
    }

    MeshDataDefinitionPtr VAMLoader::LoadMeshFromVAM(const std::string& vamPath)
    {
        try
        {
            std::ifstream file(vamPath, std::ios::binary | std::ios::in);
            if (!file.is_open())
            {
                VA_ENGINE_ERROR("[VAMLoader] Failed to open VAM file for reading: {}", vamPath);
                return nullptr;
            }

            // Read and validate header
            VAMHeader header{};
            file.read(reinterpret_cast<char*>(reinterpret_cast<uint8_t*>(&header)), sizeof(header));
            if (!header.IsValid())
            {
                VA_ENGINE_ERROR("[VAMLoader] Invalid VAM header in file: {}", vamPath);
                return nullptr;
            }

            auto meshData = new MeshDataDefinition();

            if (header.IsCompressed())
            {
                // Handle compressed VAM file
                if (!VAMCompression::IsLZ4Available())
                {
                    VA_ENGINE_ERROR(
                        "[VAMLoader] Compressed VAM requires LZ4 but it's not available: {}",
                        vamPath);
                    delete meshData;
                    return nullptr;
                }

                // Read compressed data
                VAArray<uint8_t> compressedData(header.stringTableSize);
                file.read(reinterpret_cast<char*>(compressedData.data()), header.stringTableSize);
                file.close();

                // Decompress all data
                auto decompressedData = VAMCompression::Decompress(
                    compressedData,
                    header.uncompressedSize);
                if (decompressedData.empty())
                {
                    VA_ENGINE_ERROR("[VAMLoader] Failed to decompress VAM: {}", vamPath);
                    delete meshData;
                    return nullptr;
                }

                // Parse decompressed data sections
                uint32_t offset = 0;

                // Extract string table
                VAArray<uint8_t> stringTable(header.stringTableSize);
                if (header.originalStringTableSize > 0)
                {
                    memcpy(
                        stringTable.data(),
                        decompressedData.data() + offset,
                        header.stringTableSize);
                    offset += header.originalStringTableSize;
                }

                // Extract vertices
                meshData->m_Vertices.resize(header.vertexCount);
                for (uint32_t i = 0; i < header.vertexCount; ++i)
                {
                    VAMVertex vamVertex;
                    memcpy(&vamVertex, decompressedData.data() + offset, sizeof(VAMVertex));
                    offset += sizeof(VAMVertex);

                    // Convert to engine format
                    MeshVertex& vertex = meshData->m_Vertices[i];
                    vertex.Position = vamVertex.position;
                    vertex.Normal = vamVertex.normal;
                    vertex.UV0 = vamVertex.uv0;
                    vertex.Tangent = vamVertex.tangent;
                }

                // Extract indices
                meshData->m_Indices.resize(header.indexCount);
                if (header.originalIndicesSize > 0)
                {
                    memcpy(
                        meshData->m_Indices.data(),
                        decompressedData.data() + offset,
                        header.originalIndicesSize);
                    offset += header.originalIndicesSize;
                }

                // Extract submeshes
                VAArray<VAMSubMeshDescriptor> vamSubmeshes(header.submeshCount);
                for (uint32_t i = 0; i < header.submeshCount; ++i)
                {
                    memcpy(
                        &vamSubmeshes[i],
                        decompressedData.data() + offset,
                        sizeof(VAMSubMeshDescriptor));
                    offset += sizeof(VAMSubMeshDescriptor);
                }

                // Extract materials
                VAArray<VAMMAterialTemplate> vamMaterials(header.materialCount);
                for (uint32_t i = 0; i < header.materialCount; ++i)
                {
                    memcpy(
                        &vamMaterials[i],
                        decompressedData.data() + offset,
                        sizeof(VAMMAterialTemplate));
                    offset += sizeof(VAMMAterialTemplate);
                }

                // Calculate total bindings and extract them
                VAArray<VAMResourceBinding> allBindings;
                if (header.originalBindingsSize > 0)
                {
                    auto bindingCount = header.originalBindingsSize / sizeof(VAMResourceBinding);
                    allBindings.resize(bindingCount);
                    memcpy(
                        allBindings.data(),
                        decompressedData.data() + offset,
                        header.originalBindingsSize);
                    offset += header.originalBindingsSize;
                }

                // Verify we consumed all data
                if (offset != header.uncompressedSize)
                {
                    VA_ENGINE_WARN(
                        "[VAMLoader] Data size mismatch during decompression (expected {}, consumed {}).",
                        header.uncompressedSize,
                        offset);
                }

                // Convert submeshes and restore materials
                meshData->m_Submeshes.reserve(header.submeshCount);
                for (const auto& vamSubmesh : vamSubmeshes)
                {
                    auto submeshName = ReadFromStringTable(stringTable, vamSubmesh.nameOffset);

                    SubMeshDescriptor submesh;
                    submesh.name = submeshName;
                    submesh.material = vamSubmesh.materialIndex; // Will be converted later
                    submesh.indexOffset = vamSubmesh.indexOffset;
                    submesh.indexCount = vamSubmesh.indexCount;
                    submesh.vertexOffset = vamSubmesh.vertexOffset;
                    submesh.vertexCount = vamSubmesh.vertexCount;

                    meshData->m_Submeshes.push_back(submesh);
                }

                // Restore materials and update submesh material handles
                RestoreMaterialFromVAM(
                    vamMaterials,
                    allBindings,
                    stringTable,
                    meshData->m_Submeshes);

                VA_ENGINE_INFO(
                    "[VAMLoader] Loaded compressed VAM: {} ({} vertices, {} indices, {} submeshes, {} materials) [{:.1f}% compression].",
                    vamPath,
                    header.vertexCount,
                    header.indexCount,
                    header.submeshCount,
                    header.materialCount,
                    header.GetCompressionRatio() * 100.0f);
            }
            else
            {
                // Read string table
                VAArray<uint8_t> stringTable(header.stringTableSize);
                if (header.stringTableSize > 0)
                {
                    file.read(reinterpret_cast<char*>(stringTable.data()), header.stringTableSize);
                }

                // Read vertices
                meshData->m_Vertices.resize(header.vertexCount);
                for (uint32_t i = 0; i < header.vertexCount; ++i)
                {
                    VAMVertex vamVertex;
                    file.read(reinterpret_cast<char*>(&vamVertex), sizeof(vamVertex));

                    MeshVertex& vertex = meshData->m_Vertices[i];
                    vertex.Position = vamVertex.position;
                    vertex.Normal = vamVertex.normal;
                    vertex.UV0 = vamVertex.uv0;
                    vertex.Tangent = vamVertex.tangent;
                }

                // Read indices
                meshData->m_Indices.resize(header.indexCount);
                if (header.indexCount > 0)
                {
                    file.read(
                        reinterpret_cast<char*>(meshData->m_Indices.data()),
                        header.indexCount * sizeof(uint32_t));
                }

                // Read submeshes
                VAArray<VAMSubMeshDescriptor> vamSubmeshes(header.submeshCount);
                for (uint32_t i = 0; i < header.submeshCount; ++i)
                {
                    file.read(reinterpret_cast<char*>(&vamSubmeshes[i]), sizeof(vamSubmeshes[i]));
                }

                // Read materials
                VAArray<VAMMAterialTemplate> vamMaterials(header.materialCount);
                for (uint32_t i = 0; i < header.materialCount; ++i)
                {
                    file.read(reinterpret_cast<char*>(&vamMaterials[i]), sizeof(vamMaterials[i]));
                }

                // Calculate total bindings
                uint32_t totalBindings = 0;
                for (const auto& mat : vamMaterials)
                {
                    totalBindings += mat.bindingCount;
                }

                // Read resource bindings
                VAArray<VAMResourceBinding> allBindings(totalBindings);
                if (totalBindings > 0)
                {
                    file.read(
                        reinterpret_cast<char*>(allBindings.data()),
                        totalBindings * sizeof(VAMResourceBinding));
                }

                file.close();

                // Convert VAM submeshes to engine format
                meshData->m_Submeshes.reserve(header.submeshCount);
                for (const auto& vamSubmesh : vamSubmeshes)
                {
                    auto submeshName = ReadFromStringTable(stringTable, vamSubmesh.nameOffset);

                    SubMeshDescriptor submesh;
                    submesh.name = submeshName;
                    submesh.material = vamSubmesh.materialIndex; // Will be converted later
                    submesh.indexOffset = vamSubmesh.indexOffset;
                    submesh.indexCount = vamSubmesh.indexCount;
                    submesh.vertexOffset = vamSubmesh.vertexOffset;
                    submesh.vertexCount = vamSubmesh.vertexCount;

                    meshData->m_Submeshes.push_back(submesh);
                }

                // Restore materials and update submesh material handles
                RestoreMaterialFromVAM(
                    vamMaterials,
                    allBindings,
                    stringTable,
                    meshData->m_Submeshes);

                VA_ENGINE_TRACE(
                    "[VAMLoader] Successfully loaded VAM: {} ({} vertices, {} indices, {} submeshes, {} materials).",
                    vamPath,
                    header.vertexCount,
                    header.indexCount,
                    header.submeshCount,
                    header.materialCount);
            }

            return MeshDataDefinitionPtr(meshData);
        }
        catch (const std::exception& ex)
        {
            VA_ENGINE_ERROR("[VAMLoader] Failed to load VAM: {} : {}", vamPath, ex.what());
            return nullptr;
        }
    }

    bool VAMLoader::IsVAMValid(const std::string& vamPath, const std::string& sourcePath)
    {
        // Check if VAM file exists
        if (!std::filesystem::exists(vamPath))
        {
            return false;
        }

        try
        {
            // Quick header validation
            std::fstream file(vamPath, std::ios::binary | std::ios::in);
            if (!file.is_open())
            {
                return false;
            }
            VAMHeader header{};
            file.read(reinterpret_cast<char*>(reinterpret_cast<uint8_t*>(&header)), sizeof(header));
            file.close();

            if (!header.IsValid())
            {
                VA_ENGINE_WARN("[VAMLoader] Invalid VAM header.");
                return false;
            }

            // Check if source exists (needed for timestamp comp)
            if (sourcePath.empty() || !std::filesystem::exists(sourcePath))
            {
                // If source doesn't exist but VAM does, consider VAM valid
                // This supports runtime-only builds in the future
                return true;
            }

            auto sourceTime = std::filesystem::last_write_time(sourcePath);

            // Convert using the same method as in SaveMeshToVAM
            auto duration = sourceTime.time_since_epoch();
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
            auto currentSourceTimestamp = static_cast<uint64_t>(seconds.count());

            if (header.sourceTimestamp == 0)
            {
                VA_ENGINE_WARN("[VAMLoader] VAM has no source timestamp.");
                return false;
            }

            if (currentSourceTimestamp > header.sourceTimestamp)
            {
                VA_ENGINE_TRACE("[VAMLoader] Source file newer than VAM cache.");
                return false;
            }

            VA_ENGINE_TRACE(
                "[VAMLoader] VAM cache valid (source: {}, vam: {}).",
                currentSourceTimestamp,
                header.sourceTimestamp);

            return true;
        }
        catch (const std::exception& ex)
        {
            VA_ENGINE_WARN("[VAMLoader] Error validating VAM cache: {}", ex.what());
            return false;
        }
    }

    std::string VAMLoader::GetVAMPath(const std::string& sourcePath) const
    {
        return m_BaseAssetPath + "../cache/" + sourcePath + ".vam";
    }

    std::string VAMLoader::FindSourceAsset(const std::string& name) const
    {
        static constexpr std::string extensions[] = {".gltf", ".fbx", ".obj"};

        for (const auto& extension : extensions)
        {
            std::string path = m_BaseAssetPath + name;
            path += extension;
            if (std::filesystem::exists(path))
            {
                return path;
            }
        }

        return "";
    }

    MeshDataDefinitionPtr VAMLoader::ImportAndBake(const std::string& name) const
    {
        // Use raw loader to import from source file
        auto meshData = std::dynamic_pointer_cast<MeshDataDefinition>(m_RawMeshLoader->Load(name));
        if (!meshData)
        {
            VA_ENGINE_ERROR("[VAMLoader] Failed to import mesh '{}'.", name);
            return nullptr;
        }

        // Bake to VAM for future loads
        auto vamPath = GetVAMPath(name);
        if (SaveMeshToVAM(
            vamPath,
            FindSourceAsset(std::filesystem::path(vamPath).stem().string()),
            *meshData,
            m_CompressionSettings))
        {
            VA_ENGINE_TRACE("[VAMLoader] Successfully baked mesh '{}' to VAM.", name);
        }
        else
        {
            VA_ENGINE_WARN("[VAMLoader] Failed to bake mesh '{}' to VAM.", name);
        }

        return meshData;
    }

    uint32_t VAMLoader::AddToStringTable(
        const std::string& str,
        VAArray<uint8_t>& stringTable,
        VAHashMap<std::string, uint32_t>& stringOffsets)
    {
        // Check if string already exists
        auto it = stringOffsets.find(str);
        if (it != stringOffsets.end())
        {
            return it->second;
        }

        // Add new string
        const auto offset = static_cast<uint32_t>(stringTable.size());
        stringOffsets[str] = offset;

        // Add null-terminated string to table
        const char* cstr = str.c_str();
        for (size_t i = 0; i <= str.length(); ++i)
        {
            stringTable.push_back(static_cast<uint8_t>(cstr[i]));
        }

        return offset;
    }

    std::string VAMLoader::ReadFromStringTable(const VAArray<uint8_t>& stringTable, uint32_t offset)
    {
        if (offset >= stringTable.size())
        {
            VA_ENGINE_ERROR("[VAMLoader] Invalid string table offset: {}.", offset);
            return "";
        }

        // Read null-terminating string
        std::string result;
        for (size_t i = offset; i < stringTable.size(); ++i)
        {
            const auto c = static_cast<char>(stringTable[i]);
            if (c == '\0') break;

            result += c;
        }

        return result;
    }

    VAArray<VAMMAterialTemplate> VAMLoader::ConvertMaterialsToVAM(
        const VAArray<SubMeshDescriptor>& submeshes,
        VAArray<uint8_t>& stringTable,
        VAHashMap<std::string, uint32_t>& stringOffsets,
        VAArray<VAMResourceBinding>& allBindings)
    {
        VAArray<VAMMAterialTemplate> vamMaterials;
        VAHashMap<MaterialHandle, uint32_t> materialIndexMap;

        for (const auto& submesh : submeshes)
        {
            auto handle = submesh.material;

            // Skip if already processed
            if (materialIndexMap.contains(handle)) continue;

            // Get material template from the system
            auto& matTemplate = g_MaterialSystem->GetTemplateFor(handle);

            VAMMAterialTemplate vamMat;

            // Add strings to table
            vamMat.nameOffset = AddToStringTable(matTemplate.name, stringTable, stringOffsets);
            vamMat.renderStateClassOffset = AddToStringTable(
                matTemplate.renderStateClass,
                stringTable,
                stringOffsets);

            vamMat.diffuseColor = matTemplate.diffuseColor;

            // Add texture names
            vamMat.diffuseTextureOffset = AddToStringTable(
                matTemplate.diffuseTexture.name,
                stringTable,
                stringOffsets);
            vamMat.specularTextureOffset = AddToStringTable(
                matTemplate.specularTexture.name,
                stringTable,
                stringOffsets);
            vamMat.normalTextureOffset = AddToStringTable(
                matTemplate.normalTexture.name,
                stringTable,
                stringOffsets);

            // Convert resource bindings
            vamMat.bindingCount = static_cast<uint32_t>(matTemplate.resourceBindings.size());
            for (const auto& binding : matTemplate.resourceBindings)
            {
                VAMResourceBinding vamBinding{};
                vamBinding.type = static_cast<uint32_t>(binding.type);
                vamBinding.binding = binding.binding;
                vamBinding.stage = static_cast<uint32_t>(binding.stage);

                allBindings.push_back(vamBinding);
            }

            // Map handle to index
            materialIndexMap[handle] = static_cast<uint32_t>(vamMaterials.size());
            vamMaterials.push_back(vamMat);
        }

        return vamMaterials;
    }

    void VAMLoader::RestoreMaterialFromVAM(
        const VAArray<VAMMAterialTemplate>& vamMaterials,
        const VAArray<VAMResourceBinding>& bindings,
        const VAArray<uint8_t>& stringTable,
        VAArray<SubMeshDescriptor>& submeshes)
    {
        VAArray<MaterialHandle> materialHandles;
        materialHandles.reserve(vamMaterials.size());

        uint32_t bindingOffset = 0;

        // Restore each material
        for (const auto& vamMat : vamMaterials)
        {
            MaterialTemplate matTemplate;

            // Read strings from table
            matTemplate.name = ReadFromStringTable(stringTable, vamMat.nameOffset);
            matTemplate.renderStateClass = ReadFromStringTable(
                stringTable,
                vamMat.renderStateClassOffset);

            matTemplate.diffuseColor = vamMat.diffuseColor;

            // Read texture names
            matTemplate.diffuseTexture.name = ReadFromStringTable(
                stringTable,
                vamMat.diffuseTextureOffset);
            matTemplate.specularTexture.name = ReadFromStringTable(
                stringTable,
                vamMat.specularTextureOffset);
            matTemplate.normalTexture.name = ReadFromStringTable(
                stringTable,
                vamMat.normalTextureOffset);

            // Set texture uses (standard defaults)
            matTemplate.diffuseTexture.use = Resources::TextureUse::Diffuse;
            matTemplate.specularTexture.use = Resources::TextureUse::Specular;
            matTemplate.normalTexture.use = Resources::TextureUse::Normal;

            // Restore resource bindings
            matTemplate.resourceBindings.clear();
            for (uint32_t i = 0; i < vamMat.bindingCount; ++i)
            {
                const auto& vamBinding = bindings[bindingOffset + i];
                Renderer::ResourceBinding binding;
                binding.type = static_cast<Renderer::ResourceBindingType>(vamBinding.type);
                binding.binding = vamBinding.binding;
                binding.stage = static_cast<Resources::ShaderStage>(vamBinding.stage);

                matTemplate.resourceBindings.push_back(binding);
            }

            bindingOffset += vamMat.bindingCount;

            // Register material and get handle
            auto handle = g_MaterialSystem->RegisterTemplate(matTemplate.name, matTemplate);
            materialHandles.push_back(handle);

            VA_ENGINE_TRACE(
                "[VAMLoader] Restored material '{}' with handle {}",
                matTemplate.name,
                handle);
        }

        // Update submeshes with correct material handles
        for (auto& submesh : submeshes)
        {
            if (submesh.material < materialHandles.size())
            {
                submesh.material = materialHandles[submesh.material];
            }
            else
            {
                VA_ENGINE_WARN(
                    "[VAMLoader] Invalid material index in submesh {} : {}.",
                    submesh.material,
                    submesh.name);
                submesh.material = g_MaterialSystem->GetHandleForDefaultMaterial();
            }
        }
    }
}
