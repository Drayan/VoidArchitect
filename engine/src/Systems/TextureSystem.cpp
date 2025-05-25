//
// Created by Michael Desmedt on 25/05/2025.
//
#include "TextureSystem.hpp"

#include <ranges>
#include <stb_image.h>

#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Platform/RHI/Material.hpp"
#include "Renderer/RenderCommand.hpp"
#include "Resources/Texture.hpp"

namespace VoidArchitect
{
    TextureSystem::TextureSystem()
    {
        m_Textures.reserve(1024); // Reserve space for 1024 textures to avoid frequent reallocations
        GenerateDefaultTextures();
    }

    TextureSystem::~TextureSystem()
    {
        IMaterial::SetDefaultDiffuseTexture(nullptr);
        m_DefaultTexture = nullptr;

        if (!m_TextureCache.empty())
        {
            VA_ENGINE_WARN(
                "[TextureSystem] Texture cache is not empty during destruction. "
                "This may indicate a resource leak.");

            for (const auto& [uuid, texture] : m_TextureCache)
            {
                if (const auto tex = texture.lock())
                {
                    VA_ENGINE_WARN(
                        "[TextureSystem] Texture '{}' with UUID {} was not released properly.",
                        tex->m_Name,
                        static_cast<uint64_t>(uuid));
                    tex->Release();
                }
            }
        }

        m_TextureCache.clear();
    }

    Resources::Texture2DPtr TextureSystem::LoadTexture2D(const std::string& name)
    {
        // Check if the texture is in the cache
        for (auto& [uuid, texture] : m_TextureCache)
        {
            const auto& tex = texture.lock();
            if (tex && tex->m_Name == name)
            {
                // If the texture is found in the cache, return it
                return std::dynamic_pointer_cast<Resources::Texture2D>(tex);
            }

            if (tex == nullptr)
            {
                // Remove expired weak pointers from the cache
                m_TextureCache.erase(uuid);
            }
        }

        // If not found, load the texture from a file
        int32_t width, height, channels;
        auto hasTransparency = false;
        const auto data = LoadRawData(name, width, height, channels, hasTransparency);

        Resources::Texture2D* texture = nullptr;
        switch (Renderer::RenderCommand::GetApiType())
        {
            case Platform::RHI_API_TYPE::Vulkan:
            {
                texture = Renderer::RenderCommand::GetRHIRef().CreateTexture2D(
                    name,
                    width,
                    height,
                    4,
                    hasTransparency,
                    data);
            }
            default:
                break;
        }

        if (texture)
        {
            auto texturePtr = Resources::Texture2DPtr(
                texture,
                TextureDeleter{this});
            // Cache the texture
            const auto handle = GetFreeTextureHandle();
            texture->m_Handle = handle;

            m_Textures[handle] = {texture->m_UUID, data.size()};
            m_TextureCache[texture->m_UUID] = texturePtr;

            m_TotalMemoryUsed += data.size();
            m_TotalTexturesLoaded++;

            VA_ENGINE_TRACE(
                "[TextureSystem] Created texture '{}' with handle {}.",
                texture->m_Name,
                handle);
            return texturePtr;
        }

        VA_ENGINE_WARN("[RenderCommand] Failed to create a texture {}.", name);
        return nullptr;
    }

    Resources::Texture2DPtr TextureSystem::CreateTexture2D(
        const std::string& name,
        const uint32_t width,
        const uint32_t height,
        const uint8_t channels,
        const bool hasTransparency,
        const std::vector<uint8_t>& data)
    {
        Resources::Texture2D* texture = nullptr;
        switch (Renderer::RenderCommand::GetApiType())
        {
            case Platform::RHI_API_TYPE::Vulkan:
                texture = Renderer::RenderCommand::GetRHIRef().CreateTexture2D(
                    name,
                    width,
                    height,
                    channels,
                    hasTransparency,
                    data);
            default:
                break;
        }

        if (texture)
        {
            auto texturePtr = Resources::Texture2DPtr(
                texture,
                TextureDeleter{this});
            // Cache the texture
            const auto handle = GetFreeTextureHandle();
            texture->m_Handle = handle;

            m_Textures[handle] = {texture->m_UUID, data.size()};
            m_TextureCache[texture->m_UUID] = texturePtr;

            m_TotalMemoryUsed += data.size();
            m_TotalTexturesLoaded++;

            VA_ENGINE_TRACE(
                "[TextureSystem] Created texture '{}' with handle {}.",
                texture->m_Name,
                handle);
            return texturePtr;
        }

        VA_ENGINE_WARN("[RenderCommand] Failed to create a texture.");
        return nullptr;
    }

    void TextureSystem::GenerateDefaultTextures()
    {
        constexpr uint32_t texSize = 256;
        constexpr uint32_t texChannels = 4;

        std::vector<uint8_t> texData(texSize * texSize * texChannels);
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

        m_DefaultTexture = CreateTexture2D(
            "DefaultTexture",
            texSize,
            texSize,
            texChannels,
            false,
            texData);
        IMaterial::SetDefaultDiffuseTexture(m_DefaultTexture);
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
        if (const auto it = m_TextureCache.find(texture->m_UUID); it != m_TextureCache.end())
        {
            VA_ENGINE_TRACE("[TextureSystem] Releasing texture {}.", texture->m_Name);
            // Release the texture handle
            m_FreeTextureHandles.push(texture->m_Handle);
            const auto size = m_Textures[texture->m_Handle].dataSize;
            m_Textures[texture->m_Handle] = {InvalidUUID, 0};
            m_TextureCache.erase(it);

            m_TotalMemoryUsed -= size;
            m_TotalTexturesLoaded--;
        }
    }

    std::vector<uint8_t> TextureSystem::LoadRawData(
        const std::string& name,
        int32_t& width,
        int32_t& height,
        int32_t& channels,
        bool& hasTransparency)
    {
        std::stringstream ss;
        ss << "assets/textures/" << name << ".png";
        //TODO Try other formats like JPG, TGA, BMP, etc.

        const auto rawData = stbi_load(ss.str().c_str(), &width, &height, &channels, 4);
        if (rawData == nullptr)
        {
            VA_ENGINE_WARN(
                "[RenderCommand] Failed to load texture '{}', with error {}.",
                name,
                stbi_failure_reason());
            return {};
        }
        auto data = std::vector<uint8_t>(width * height * 4);
        memcpy(data.data(), rawData, data.size());
        stbi_image_free(rawData);

        // Search for transparency
        hasTransparency = false;
        for (size_t i = 0; i < data.size(); i += 4)
        {
            if (data[i + 3] < 255)
            {
                hasTransparency = true;
                break;
            }
        }

        return data;
    }

    void TextureSystem::TextureDeleter::operator()(const Resources::ITexture* texture) const
    {
        system->ReleaseTexture(texture);
        delete texture;
    }
}
