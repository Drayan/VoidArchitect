//
// Created by Michael Desmedt on 22/05/2025.
//
#include "Texture.hpp"

#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Platform/RHI/Vulkan/VulkanTexture.hpp"
#include "Systems/Renderer/RenderCommand.hpp"

namespace VoidArchitect::Resources
{
    std::shared_ptr<Texture2D> Texture2D::Create(const std::string& name)
    {
        return Renderer::RenderCommand::CreateTexture2D(name);
    }
} // VoidArchitect
