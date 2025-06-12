//
// Created by Michael Desmedt on 08/06/2025.
//
#include "RendererTypes.hpp"

#include "Core/Logger.hpp"

VoidArchitect::Renderer::ResourceBindingType VoidArchitect::Renderer::ResourceBindingTypeFromString(
    const std::string& str)
{
    //TODO: Support other ResourceType
    if (str == "ConstantBuffer")
    {
        return VoidArchitect::Renderer::ResourceBindingType::ConstantBuffer;
    }
    if (str == "Texture2D")
    {
        return VoidArchitect::Renderer::ResourceBindingType::Texture2D;
    }

    VA_ENGINE_WARN("[ResourceSystem] Unknown resource type, defaulting to ConstantBuffer.");
    return VoidArchitect::Renderer::ResourceBindingType::ConstantBuffer;
}
