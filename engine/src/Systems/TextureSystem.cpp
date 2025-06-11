//
// Created by Michael Desmedt on 25/05/2025.
//
#include "TextureSystem.hpp"

#include <stb_image.h>

#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "ResourceSystem.hpp"
#include "Renderer/RenderSystem.hpp"
#include "Resources/Loaders/ImageLoader.hpp"
#include "Resources/Texture.hpp"

namespace VoidArchitect
{
    TextureSystem::TextureSystem()
    {
        m_Textures.reserve(4096); // Reserve space for 1024 textures to avoid frequent reallocations
        GenerateDefaultTextures();
    }

    TextureSystem::~TextureSystem()
    {
    }

    Resources::TextureHandle TextureSystem::GetHandleFor(const std::string& name)
    {
        // Check if the texture is in the cache
        for (uint32_t i = 0; i < m_Textures.size(); i++)
        {
            if (m_Textures[i]->m_Name == name)
                return i;
        }

        // If not found, load the texture from a file
        const auto imageDefinition =
            g_ResourceSystem->LoadResource<Resources::Loaders::ImageDataDefinition>(
                ResourceType::Image,
                name);

        Resources::Texture2D* rawPtr = Renderer::g_RenderSystem->GetRHI()->CreateTexture2D(
            name,
            imageDefinition->GetWidth(),
            imageDefinition->GetHeight(),
            4,
            imageDefinition->HasTransparency(),
            imageDefinition->GetData());

        if (rawPtr)
        {
            // Cache the texture
            const auto handle = GetFreeTextureHandle();
            rawPtr->m_Handle = handle;

            m_Textures[handle] = std::unique_ptr<Resources::ITexture>(rawPtr);

            VA_ENGINE_TRACE(
                "[TextureSystem] Created texture '{}' with handle {}.",
                rawPtr->m_Name,
                handle);
            return handle;
        }

        VA_ENGINE_WARN("[RenderCommand] Failed to create a texture {}.", name);
        return Resources::InvalidTextureHandle;
    }

    Resources::TextureHandle TextureSystem::CreateTexture2D(
        const std::string& name,
        const uint32_t width,
        const uint32_t height,
        const uint8_t channels,
        const bool hasTransparency,
        const VAArray<uint8_t>& data)
    {
        Resources::Texture2D* texture = Renderer::g_RenderSystem->GetRHI()->CreateTexture2D(
            name,
            width,
            height,
            channels,
            hasTransparency,
            data);
        if (texture)
        {
            // Cache the texture
            const auto handle = GetFreeTextureHandle();
            texture->m_Handle = handle;

            m_Textures[handle] = std::unique_ptr<Resources::ITexture>(texture);

            VA_ENGINE_TRACE(
                "[TextureSystem] Created texture '{}' with handle {}.",
                texture->m_Name,
                handle);
            return handle;
        }

        VA_ENGINE_WARN("[RenderCommand] Failed to create a texture.");
        return Resources::InvalidTextureHandle;
    }

    void TextureSystem::GenerateDefaultTextures()
    {
        constexpr uint32_t texSize = 256;
        constexpr uint32_t texChannels = 4;

        VAArray<uint8_t> texData(texSize * texSize * texChannels);
        for (uint32_t y = 0; y < texSize; ++y)
        {
            for (uint32_t x = 0; x < texSize; ++x)
            {
                constexpr uint8_t magenta[4] = {255, 0, 255, 255};
                constexpr uint8_t white[4] = {255, 255, 255, 255};
                constexpr uint32_t squareSize = 32;

                const auto squareX = x / squareSize;
                const auto squareY = y / squareSize;

                const bool isWhite = (squareX + squareY) % 2 == 0;

                const auto pixelIndex = (y * texSize + x) * texChannels;

                const uint8_t* color = isWhite ? white : magenta;
                texData[pixelIndex + 0] = color[0];
                texData[pixelIndex + 1] = color[1];
                texData[pixelIndex + 2] = color[2];
                texData[pixelIndex + 3] = color[3];
            }
        }

        m_DefaultTextureHandle = CreateTexture2D(
            "DefaultTexture",
            texSize,
            texSize,
            texChannels,
            false,
            texData);
    }

    uint32_t TextureSystem::GetFreeTextureHandle()
    {
        // If there are free handles, return one
        if (!m_FreeTextureHandles.empty())
        {
            const uint32_t handle = m_FreeTextureHandles.front();
            m_FreeTextureHandles.pop();
            return handle;
        }

        // Otherwise, return the next handle and increment it
        if (m_NextTextureHandle >= m_Textures.size())
        {
            m_Textures.resize(m_NextTextureHandle + 1); // Ensure the vector can hold the new handle
        }
        return m_NextTextureHandle++;
    }

    void TextureSystem::ReleaseTexture(const Resources::ITexture* texture)
    {
    }
} // namespace VoidArchitect
