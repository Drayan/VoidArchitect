//
// Created by Michael Desmedt on 01/06/2025.
//
#pragma once

namespace VoidArchitect::Resources::Loaders
{
    class IResourceDefinition;

    class ILoader
    {
    public:
        explicit ILoader(const std::string& baseAssetPath);
        virtual ~ILoader() = default;

        virtual std::shared_ptr<IResourceDefinition> Load(const std::string& name) = 0;

    protected:
        std::string m_BaseAssetPath;
    };
}
