//
// Created by Michael Desmedt on 15/06/2025.
//
#pragma once
#include "Loader.hpp"
#include "RawMeshLoader.hpp"
#include "VamCompression.hpp"
#include "VAMFormat.hpp"

namespace VoidArchitect::Resources::Loaders
{
    class VAMLoader final : public ILoader
    {
    public:
        explicit VAMLoader(const std::string& baseAssetPath);
        ~VAMLoader() override = default;

        std::shared_ptr<IResourceDefinition> Load(const std::string& name) override;

        // Compression settings
        void SetCompressionSettings(const VAMCompressionSettings& settings)
        {
            m_CompressionSettings = settings;
        }

        const VAMCompressionSettings& GetCompressionSettings() const
        {
            return m_CompressionSettings;
        }

        static bool SaveMeshToVAM(
            const std::string& vamPath,
            const std::string& sourcePath,
            const MeshDataDefinition& meshData,
            const VAMCompressionSettings& compressionSettings);
        static MeshDataDefinitionPtr LoadMeshFromVAM(const std::string& vamPath);

        static bool IsVAMValid(const std::string& vamPath, const std::string& sourcePath);

    private:
        std::unique_ptr<RawMeshLoader> m_RawMeshLoader;
        VAMCompressionSettings m_CompressionSettings;

        std::string GetVAMPath(const std::string& sourcePath) const;
        std::string FindSourceAsset(const std::string& name) const;

        MeshDataDefinitionPtr ImportAndBake(const std::string& name) const;

        static uint32_t AddToStringTable(
            const std::string& str,
            VAArray<uint8_t>& stringTable,
            VAHashMap<std::string, uint32_t>& stringOffsets);
        static std::string ReadFromStringTable(
            const VAArray<uint8_t>& stringTable,
            uint32_t offset);

        static VAArray<VAMMAterialTemplate> ConvertMaterialsToVAM(
            const VAArray<SubMeshDescriptor>& submeshes,
            VAArray<uint8_t>& stringTable,
            VAHashMap<std::string, uint32_t>& stringOffsets,
            VAArray<VAMResourceBinding>& allBindings);

        static void RestoreMaterialFromVAM(
            const VAArray<VAMMAterialTemplate>& vamMaterials,
            const VAArray<VAMResourceBinding>& bindings,
            const VAArray<uint8_t>& stringTable,
            VAArray<SubMeshDescriptor>& submeshes);
    };
}
