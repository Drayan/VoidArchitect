//
// Created by Michael Desmedt on 25/05/2025.
//
#pragma once
#include "Core/Uuid.hpp"

#include "Resources/Texture.hpp"

namespace VoidArchitect
{
    class TextureSystem
    {
    public:
        TextureSystem();
        ~TextureSystem();

        Resources::Texture2DPtr LoadTexture2D(const std::string& name);
        Resources::Texture2DPtr CreateTexture2D(
            const std::string& name,
            uint32_t width,
            uint32_t height,
            uint8_t channels,
            bool hasTransparency,
            const std::vector<uint8_t>& data);

        void ReloadTexture(const UUID& uuid);
        void ReloadTexture(uint32_t textureHandle);
        void ReloadTexture(const Resources::TexturePtr& texture);
        void ReloadTexture(const std::string& name);
        void ReloadAllTextures();

        size_t GetTotalMemoryUsed();
        size_t GetTotalTexturesLoaded();

        Resources::Texture2DPtr GetDefaultTexture() const
        {
            return m_DefaultTexture;
        }

    private:
        void GenerateDefaultTextures();
        uint32_t GetFreeTextureHandle();
        void ReleaseTexture(const Resources::ITexture* texture);

        //TEMP This will be removed once we have a proper resource manager
        static std::vector<uint8_t> LoadRawData(
            const std::string& name,
            int32_t& width,
            int32_t& height,
            int32_t& channels,
            bool& hasTransparency);
        // TEMP End

        struct TextureDeleter
        {
            TextureSystem* system;

            void operator()(const Resources::ITexture* texture) const;
        };

        struct TextureData
        {
            UUID uuid = InvalidUUID;
            size_t dataSize = 0;
        };

        std::queue<uint32_t> m_FreeTextureHandles;
        uint32_t m_NextTextureHandle = 0;

        std::vector<TextureData> m_Textures;
        std::unordered_map<UUID, std::weak_ptr<Resources::ITexture>> m_TextureCache;

        size_t m_TotalMemoryUsed = 0;
        size_t m_TotalTexturesLoaded = 0;

        Resources::Texture2DPtr m_DefaultTexture;
    };

    inline std::unique_ptr<TextureSystem> g_TextureSystem;
}
