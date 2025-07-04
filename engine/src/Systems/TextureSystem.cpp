//
// Created by Michael Desmedt on 25/05/2025.
//
#include "TextureSystem.hpp"

#include <stb_image.h>

#include "Core/Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "ResourceSystem.hpp"
#include "Jobs/JobSystem.hpp"
#include "Renderer/RenderSystem.hpp"
#include "Resources/Loaders/ImageLoader.hpp"
#include "Resources/Texture.hpp"

namespace VoidArchitect
{
    TextureSystem::TextureSystem()
    {
        // Reserve space for 1024 textures to avoid frequent reallocations
        m_TextureNodes.reserve(1024);
        GenerateDefaultTextures();
    }

    TextureSystem::~TextureSystem()
    {
        m_TextureNodes.clear();
    }

    Resources::TextureHandle TextureSystem::GetHandleFor(const std::string& name)
    {
        // Check existing texture nodes
        for (Resources::TextureHandle handle = 0; handle < m_TextureNodes.size(); ++handle)
        {
            auto& node = m_TextureNodes[handle];
            if (node.name == name)
            {
                // If loading, check if async operation completed
                if (node.state == TextureLoadState::Loading)
                {
                    if (Jobs::g_JobSystem && Jobs::g_JobSystem->IsSignaled(node.loadingComplete))
                    {
                        auto result = Jobs::g_JobSystem->GetSyncPointStatus(node.loadingComplete);
                        if (result == Jobs::JobResult::Status::Success)
                        {
                        }
                        else
                        {
                            // Async loading failed - mark as failed (GetPointerFro will handle fallback)
                            node.state = TextureLoadState::Failed;

                            VA_ENGINE_ERROR("[TextureSystem] Failed to load texture '{}'.", name);
                        }
                    }
                }

                return handle;
            }
        }

        // First time request - create new texture node and start async loading
        auto handle = CreateTextureNode(name);
        StartAsyncTextureLoading(handle);
        return handle;
    }

    Resources::ITexture* TextureSystem::GetPointerFor(const Resources::TextureHandle handle) const
    {
        if (handle >= m_TextureNodes.size())
        {
            VA_ENGINE_ERROR("[TextureSystem] Invalid texture handle.");
            return m_TextureNodes[m_ErrorTextureHandle].texturePtr.get();
        }

        const auto& node = m_TextureNodes[handle];

        // Return actual texture if loaded
        if (node.texturePtr)
        {
            return node.texturePtr.get();
        }
        else
        {
            // Handle different states appropriately
            switch (node.state)
            {
                case TextureLoadState::Failed:
                    return m_TextureNodes[m_ErrorTextureHandle].texturePtr.get();

                case TextureLoadState::Loading:
                case TextureLoadState::Unloaded: default:
                    return m_TextureNodes[m_DefaultDiffuseHandle].texturePtr.get();
            }
        }
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

            // Ensure texture node array can accomodate the handle
            if (handle >= m_TextureNodes.size())
            {
                m_TextureNodes.resize(handle + 1);
            }

            // Initialize texture node with the created texture
            auto& node = m_TextureNodes[handle];
            node.name = name;
            node.state = TextureLoadState::Loaded;
            node.texturePtr.reset(texture);
            node.loadingComplete = Jobs::InvalidSyncPointHandle;

            // Set handle in texture object
            texture->m_Handle = handle;

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

        // ==============================================================
        // ERROR TEXTURE (Checkerboard magenta/white)
        // ==============================================================
        {
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

            m_ErrorTextureHandle = CreateTexture2D(
                "ErrorTexture",
                texSize,
                texSize,
                texChannels,
                false,
                texData);

            VA_ENGINE_TRACE("[TextureSystem] Create error texture.");
        }

        // ==========================================================
        // DEFAULT DIFFUSE (white)
        // ==========================================================
        {
            VAArray<uint8_t> texData(texSize * texSize * texChannels);
            constexpr uint8_t white[4] = {255, 255, 255, 255};

            for (uint32_t i = 0; i < texSize * texSize; ++i)
            {
                const uint32_t pixelIndex = i * texChannels;
                texData[pixelIndex + 0] = white[0];
                texData[pixelIndex + 1] = white[1];
                texData[pixelIndex + 2] = white[2];
                texData[pixelIndex + 3] = white[3];
            }

            m_DefaultDiffuseHandle = CreateTexture2D(
                "DefaultDiffuse",
                texSize,
                texSize,
                texChannels,
                false,
                texData);

            VA_ENGINE_TRACE("[TextureSystem] Created default diffuse texture (white).");
        }

        // ==========================================================
        // DEFAULT SPECULAR (black)
        // ==========================================================
        {
            VAArray<uint8_t> texData(texSize * texSize * texChannels);
            constexpr uint8_t black[4] = {0, 0, 0, 255};

            for (uint32_t i = 0; i < texSize * texSize; ++i)
            {
                const uint32_t pixelIndex = i * texChannels;
                texData[pixelIndex + 0] = black[0];
                texData[pixelIndex + 1] = black[1];
                texData[pixelIndex + 2] = black[2];
                texData[pixelIndex + 3] = black[3];
            }

            m_DefaultSpecularHandle = CreateTexture2D(
                "DefaultSpecular",
                texSize,
                texSize,
                texChannels,
                false,
                texData);

            VA_ENGINE_TRACE("[TextureSystem] Created default specular texture (black).");
        }

        // ==========================================================
        // DEFAULT NORMAL (neutral blue)
        // ==========================================================µ
        {
            VAArray<uint8_t> texData(texSize * texSize * texChannels);
            constexpr uint8_t normalBlue[4] = {127, 127, 255, 255}; // (0, 0, 1) in tangent space

            for (uint32_t i = 0; i < texSize * texSize; ++i)
            {
                const uint32_t pixelIndex = i * texChannels;
                texData[pixelIndex + 0] = normalBlue[0];
                texData[pixelIndex + 1] = normalBlue[1];
                texData[pixelIndex + 2] = normalBlue[2];
                texData[pixelIndex + 3] = normalBlue[3];
            }

            m_DefaultNormalHandle = CreateTexture2D(
                "DefaultNormal",
                texSize,
                texSize,
                texChannels,
                false,
                texData);

            VA_ENGINE_TRACE("[TextureSystem] Created default normal texture (flat blue).");
        }
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
        if (m_NextTextureHandle >= m_TextureNodes.size())
        {
            m_TextureNodes.resize(m_NextTextureHandle + 1);
            // Ensure the vector can hold the new handle
        }
        return m_NextTextureHandle++;
    }

    void TextureSystem::ReleaseTexture(const Resources::ITexture* texture)
    {
    }

    void TextureSystem::StartAsyncTextureLoading(Resources::TextureHandle handle)
    {
        if (!Jobs::g_JobSystem)
        {
            VA_ENGINE_ERROR(
                "[TextureSystem] Failed to start async texture loading - no job system.");
            return;
        }

        auto& node = m_TextureNodes[handle];

        // Create SyncPoint for the complete loading pipeline
        auto completionSP = Jobs::g_JobSystem->CreateSyncPoint(1, "TextureLoaded");
        node.loadingComplete = completionSP;
        node.state = TextureLoadState::Loading;

        // Job 1: Load from disk (any worker thread)
        auto diskJobSP = Jobs::g_JobSystem->CreateSyncPoint(1, "TextureDiskLoad");
        auto diskJob = CreateTextureLoadJob(node.name);
        Jobs::g_JobSystem->Submit(diskJob, diskJobSP, Jobs::JobPriority::Normal, "TextureDiskLoad");

        // Job 2: GPU upload (main thread)
        auto gpuJob = CreateTextureUploadJob(node.name, handle);
        Jobs::g_JobSystem->SubmitAfter(
            diskJobSP,
            gpuJob,
            completionSP,
            Jobs::JobPriority::High,
            "TextureGPUUpload",
            Jobs::MAIN_THREAD_ONLY);

        VA_ENGINE_TRACE("[TextureSystem] Started async texture loading for '{}'.", node.name);
    }

    Resources::TextureHandle TextureSystem::CreateTextureNode(const std::string& name)
    {
        auto handle = GetFreeTextureHandle();

        // Ensure texture nodes array can accomodate the new handle
        if (handle >= m_TextureNodes.size())
        {
            m_TextureNodes.resize(handle + 1);
        }

        // Initialize the new texture node
        auto& node = m_TextureNodes[handle];
        node.name = name;
        node.state = TextureLoadState::Unloaded;
        node.texturePtr = nullptr;
        node.loadingComplete = Jobs::InvalidSyncPointHandle;

        return handle;
    }

    Jobs::JobFunction TextureSystem::CreateTextureLoadJob(const std::string& textureName)
    {
        return [textureName, this]() -> Jobs::JobResult
        {
            try
            {
                // Load image data via ResourceSystem
                auto imageDefinition = g_ResourceSystem->LoadResource<
                    Resources::Loaders::ImageDataDefinition>(ResourceType::Image, textureName);

                if (!imageDefinition)
                {
                    return Jobs::JobResult::Failed("Failed to load image definition.");
                }

                // Create loading data container
                auto loadingData = std::make_unique<TextureLoadingData>();
                loadingData->name = textureName;
                loadingData->width = imageDefinition->GetWidth();
                loadingData->height = imageDefinition->GetHeight();
                loadingData->channels = imageDefinition->GetBPP();
                loadingData->hasTransparency = imageDefinition->HasTransparency();

                // Move pixels data (no copy)
                auto pixelDataSize = loadingData->width * loadingData->height * loadingData->
                    channels;
                loadingData->data = std::make_unique<uint8_t[]>(pixelDataSize);

                // Copy pixel data from image definition
                const auto& sourceData = imageDefinition->GetData();
                std::memcpy(
                    loadingData->data.get(),
                    sourceData.data(),
                    std::min(pixelDataSize, static_cast<uint32_t>(sourceData.size())));

                // Store in thread-safe container for main thread pickup
                m_LoadingStorage.StoreCompletedLoad(std::move(loadingData));

                VA_ENGINE_TRACE(
                    "[TextureSystem] Completed texture disk load for '{}'.",
                    textureName);
                return Jobs::JobResult::Success();
            }
            catch (const std::exception& e)
            {
                VA_ENGINE_ERROR(
                    "[TextureSystem] Failed to load texture '{}' : {}.",
                    textureName,
                    e.what());
                return Jobs::JobResult::Failed(e.what());
            }
        };
    }

    Jobs::JobFunction TextureSystem::CreateTextureUploadJob(
        const std::string& textureName,
        Resources::TextureHandle handle)
    {
        return [textureName, handle, this]() -> Jobs::JobResult
        {
            // This job runs on the main thread, so GPU operations are safe

            // Extract loaded data from shared storage
            auto loadedData = m_LoadingStorage.RetrieveCompletedLoad(textureName);
            if (!loadedData)
            {
                VA_ENGINE_ERROR(
                    "[TextureSystem] Failed to retrieve completed texture load for '{}'.",
                    textureName);

                // Update texture node state
                if (handle <= m_TextureNodes.size())
                {
                    m_TextureNodes[handle].state = TextureLoadState::Failed;
                }

                return Jobs::JobResult::Failed("Failed to retrieve completed texture load.");
            }

            // Create the GPU texture using the loaded data
            auto gpuTexture = Renderer::g_RenderSystem->GetRHI()->CreateTexture2D(
                loadedData->name,
                loadedData->width,
                loadedData->height,
                loadedData->channels,
                loadedData->hasTransparency,
                VAArray<uint8_t>(
                    loadedData->data.get(),
                    loadedData->data.get() + (loadedData->width * loadedData->height * loadedData->
                        channels)));

            // Update texture node state based on an upload result
            if (handle < m_TextureNodes.size())
            {
                auto& node = m_TextureNodes[handle];
                if (gpuTexture)
                {
                    // Success - store the texture and update state
                    gpuTexture->m_Handle = handle;
                    node.texturePtr.reset(gpuTexture);
                    node.state = TextureLoadState::Loaded;

                    VA_ENGINE_TRACE(
                        "[TextureSystem] Completed texture GPU upload for '{}'.",
                        textureName);
                    return Jobs::JobResult::Success();
                }
                else
                {
                    // GPU upload failed - mark as failed (GetPointerFor will return error texture)
                    node.state = TextureLoadState::Failed;

                    VA_ENGINE_ERROR(
                        "[TextureSystem] Failed to upload texture to GPU for '{}'.",
                        textureName);
                    return Jobs::JobResult::Failed("Failed to upload texture to GPU.");
                }
            }
            else
            {
                VA_ENGINE_ERROR(
                    "[TextureSystem] Invalid texture handle for texture '{}'.",
                    textureName);
                return Jobs::JobResult::Failed("Invalid texture handle.");
            }

            // loadedData destructor automatically frees pixel data
        };
    }

    void TextureLoadingStorage::StoreCompletedLoad(std::unique_ptr<TextureLoadingData> data)
    {
        if (!data)
        {
            VA_ENGINE_WARN("[TextureLoadingStorage] Attempted to store null texture data.");
            return;
        }

        std::lock_guard<std::mutex> lock(m_Mutex);
        m_CompletedLoads[data->name] = std::move(data);
    }

    std::unique_ptr<TextureLoadingData> TextureLoadingStorage::RetrieveCompletedLoad(
        const std::string& name)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);

        auto it = m_CompletedLoads.find(name);
        if (it != m_CompletedLoads.end())
        {
            auto data = std::move(it->second);
            m_CompletedLoads.erase(it);
            return data;
        }

        return nullptr;
    }
} // namespace VoidArchitect
