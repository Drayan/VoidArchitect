//
// Created by Michael Desmedt on 20/05/2025.
//
#pragma once

#include "Resources/Material.hpp"

#include <vulkan/vulkan.h>

namespace VoidArchitect::Platform
{
    class VulkanRHI;
    class VulkanShader;
    class VulkanDevice;
    class VulkanSwapchain;
    class VulkanRenderPass;
    class VulkanPipeline;
    class VulkanBuffer;

    class VulkanMaterial : public Resources::IMaterial
    {
    public:
        VulkanMaterial(const std::string& name, const MaterialTemplate& config);
        ~VulkanMaterial() override = default;

        [[nodiscard]] bool HasResourcesChanged() const override;
        void MarkResourcesUpdated() override;

        void SetDiffuseColor(const Math::Vec4& color) override;
        void SetTexture(Resources::TextureUse use, Resources::TextureHandle texture) override;

        [[nodiscard]] const MaterialTemplate& GetTemplate() const { return m_Template; }

        [[nodiscard]] const Resources::MaterialUniformObject& GetUniformData() const
        {
            return m_UniformData;
        }

        [[nodiscard]] Resources::TextureHandle GetTexture(Resources::TextureUse use) const;
        [[nodiscard]] bool IsDirty() const { return m_IsDirty; }
        void ClearDirtyFlag() { m_IsDirty = false; }

    private:
        MaterialTemplate m_Template;

        Resources::MaterialUniformObject m_UniformData;
        VAHashMap<Resources::TextureUse, Resources::TextureHandle> m_Textures;

        /// @brief Cached texture generations for resource change detection
        ///
        /// Stores the last known generation of each texture handle to detect
        /// when handles change due to reallocation or other operations
        VAHashMap<Resources::TextureUse, uint32_t> m_CachedTextureGenerations;

        /// @brief Cached texture pointers for resource change detection
        ///
        /// Stores the last known texture pointer for each texture use to detect
        /// when async loading completes and replaces placeholder textures.
        VAHashMap<Resources::TextureUse, const Resources::ITexture*> m_CachedTexturePointers;

        bool m_IsDirty = true;
    };
} // namespace VoidArchitect::Platform
