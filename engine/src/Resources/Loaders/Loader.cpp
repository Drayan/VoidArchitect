#include "Loader.hpp"
//
// Created by Michael Desmedt on 01/06/2025.
//
namespace VoidArchitect::Resources::Loaders
{
    ILoader::ILoader(const std::string& baseAssetPath) : m_BaseAssetPath(baseAssetPath) {}
} // namespace VoidArchitect::Resources::Loaders
