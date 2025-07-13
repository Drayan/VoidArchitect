//
// Created by Michael Desmedt on 01/06/2025.
//
#pragma once
#include "Loader.hpp"
#include "ResourceDefinition.hpp"
#include "Systems/MaterialSystem.hpp"

namespace VoidArchitect
{
    namespace Resources
    {
        namespace Loaders
        {
            class MaterialDataDefinition final : public IResourceDefinition
            {
                friend class MaterialLoader;

            public:
                ~MaterialDataDefinition() override = default;

                MaterialTemplate& GetConfig() { return m_MaterialConfig; }

            private:
                explicit MaterialDataDefinition(const MaterialTemplate& config);

                MaterialTemplate m_MaterialConfig;
            };

            using MaterialDataDefinitionPtr = std::shared_ptr<MaterialDataDefinition>;

            class MaterialLoader final : public ILoader
            {
            public:
                explicit MaterialLoader(const std::string& baseAssetPath);
                ~MaterialLoader() override = default;

                std::shared_ptr<IResourceDefinition> Load(const std::string& name) override;
            };
        } // namespace Loaders
    } // namespace Resources
} // namespace VoidArchitect
