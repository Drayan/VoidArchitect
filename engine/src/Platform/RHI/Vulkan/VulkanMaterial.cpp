//
// Created by Michael Desmedt on 20/05/2025.
//
#include "VulkanMaterial.hpp"

#include "VulkanBuffer.hpp"
#include "VulkanExecutionContext.hpp"
#include "VulkanTexture.hpp"
#include "VulkanUtils.hpp"
#include "Systems/TextureSystem.hpp"

namespace VoidArchitect::Platform
{
    VulkanMaterial::VulkanMaterial(const std::string& name, const MaterialTemplate& config)
        : IMaterial(name),
          m_Template(config)
    {
        m_UniformData.DiffuseColor = m_Template.diffuseColor;

        VA_ENGINE_DEBUG("[VulkanMaterial] Material '{}' created.", m_Name);
    }

    bool VulkanMaterial::HasResourcesChanged() const
    {
        for (const auto& [use, handle] : m_Textures)
        {
            // Check if handle generation changed
            if (const auto it = m_CachedTextureGenerations.find(use); it ==
                m_CachedTextureGenerations.end() || it->second != handle.GetGeneration())
            {
                return true;
            }

            // Check if texture pointer changed (async loading completion)
            auto* currentTexture = g_TextureSystem->GetPointerFor(handle);
            if (const auto it = m_CachedTexturePointers.find(use); it == m_CachedTexturePointers.
                end() || it->second != currentTexture)
            {
                return true;
            }
        }

        return false;
    }

    void VulkanMaterial::MarkResourcesUpdated()
    {
        for (const auto& [use, handle] : m_Textures)
        {
            m_CachedTextureGenerations[use] = handle.GetGeneration();
            m_CachedTexturePointers[use] = g_TextureSystem->GetPointerFor(handle);
        }
    }

    void VulkanMaterial::SetDiffuseColor(const Math::Vec4& color)
    {
        if (m_UniformData.DiffuseColor != color)
        {
            m_UniformData.DiffuseColor = color;
            m_IsDirty = true;
            m_Generation++;
        }
    }

    void VulkanMaterial::SetTexture(
        const Resources::TextureUse use,
        const Resources::TextureHandle texture)
    {
        if (const auto it = m_Textures.find(use); it == m_Textures.end() || it->second != texture)
        {
            m_Textures[use] = texture;
            m_IsDirty = true;
            m_Generation++;
        }
    }

    Resources::TextureHandle VulkanMaterial::GetTexture(const Resources::TextureUse use) const
    {
        if (const auto it = m_Textures.find(use); it != m_Textures.end())
        {
            return it->second;
        }
        return Resources::InvalidTextureHandle;
    }
} // namespace VoidArchitect::Platform
