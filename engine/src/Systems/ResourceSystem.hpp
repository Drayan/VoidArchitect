//
// Created by Michael Desmedt on 01/06/2025.
//
#pragma once

#include "Resources/Loaders/Loader.hpp"

namespace VoidArchitect
{
    enum class ResourceType
    {
        Image,
        Material,
        Shader
    };

    class ResourceSystem
    {
    public:
        ResourceSystem();
        ~ResourceSystem() = default;

        void RegisterLoader(ResourceType type, Resources::Loaders::ILoader* loader);
        void UnregisterLoader(ResourceType type);

        template <typename T>
        std::shared_ptr<T> LoadResource(ResourceType type, const std::string& path);

    private:
        static std::string ResourceTypeToString(ResourceType type);

        VAHashMap<ResourceType, std::unique_ptr<Resources::Loaders::ILoader>> m_Loaders;
    };

    inline std::unique_ptr<ResourceSystem> g_ResourceSystem;
} // namespace VoidArchitect
