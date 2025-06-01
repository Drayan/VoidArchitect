//
// Created by Michael Desmedt on 01/06/2025.
//
#pragma once
#include "Loader.hpp"
#include "ResourceDefinition.hpp"
#include "Systems/ShaderSystem.hpp"

namespace VoidArchitect
{
    namespace Resources
    {
        namespace Loaders
        {
            class ShaderDataDefinition final : public IResourceDefinition
            {
                friend class ShaderLoader;

            public:
                ~ShaderDataDefinition() override = default;

                ShaderConfig& GetConfig() { return m_ShaderConfig; }
                std::vector<uint8_t>& GetCode() { return m_Code; }

            private:
                explicit ShaderDataDefinition(
                    const ShaderConfig& config,
                    std::vector<uint8_t> code);

                ShaderConfig m_ShaderConfig;
                std::vector<uint8_t> m_Code;
            };

            using ShaderDataDefinitionPtr = std::shared_ptr<ShaderDataDefinition>;

            class ShaderLoader final : public ILoader
            {
            public:
                explicit ShaderLoader(const std::string& baseAssetPath);
                ~ShaderLoader() override = default;

                std::shared_ptr<IResourceDefinition> Load(const std::string& name) override;
                ShaderConfig ParseShaderMetadata(const std::string& path);
                ShaderConfig InferMetadataFromFilename(const std::string& name);
                ShaderStage StringToShaderStage(const std::string& stage);
            };
        } // Loaders
    } // Resources
} // VoidArchitect
