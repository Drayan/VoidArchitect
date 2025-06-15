//
// Created by Michael Desmedt on 25/05/2025.
//
#pragma once

#include "Resources/Texture.hpp"

namespace VoidArchitect
{
    class TextureSystem
    {
    public:
        TextureSystem();
        ~TextureSystem();

        Resources::TextureHandle GetHandleFor(const std::string& name);

        Resources::ITexture* GetPointerFor(Resources::TextureHandle handle) const;

        Resources::TextureHandle GetDefaultDiffuseHandle() const { return m_DefaultDiffuseHandle; }
        Resources::TextureHandle GetDefaultNormalHandle() const { return m_DefaultNormalHandle; }

        Resources::TextureHandle GetDefaultSpecularHandle() const
        {
            return m_DefaultSpecularHandle;
        }

        Resources::TextureHandle GetErrorTextureHandle() const { return m_ErrorTextureHandle; }

    private:
        Resources::TextureHandle CreateTexture2D(
            const std::string& name,
            uint32_t width,
            uint32_t height,
            uint8_t channels,
            bool hasTransparency,
            const VAArray<uint8_t>& data);
        void GenerateDefaultTextures();
        uint32_t GetFreeTextureHandle();
        void ReleaseTexture(const Resources::ITexture* texture);

        std::queue<uint32_t> m_FreeTextureHandles;
        uint32_t m_NextTextureHandle = 0;

        VAArray<std::unique_ptr<Resources::ITexture>> m_Textures;

        Resources::TextureHandle m_DefaultDiffuseHandle;
        Resources::TextureHandle m_DefaultNormalHandle;
        Resources::TextureHandle m_DefaultSpecularHandle;
        Resources::TextureHandle m_ErrorTextureHandle;
    };

    inline std::unique_ptr<TextureSystem> g_TextureSystem;
} // namespace VoidArchitect
