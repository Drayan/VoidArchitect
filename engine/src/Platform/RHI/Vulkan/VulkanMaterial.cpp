//
// Created by Michael Desmedt on 20/05/2025.
//
#include "VulkanMaterial.hpp"

#include "VulkanBuffer.hpp"
#include "VulkanExecutionContext.hpp"
#include "VulkanTexture.hpp"
#include "VulkanUtils.hpp"

namespace VoidArchitect::Platform
{
    VulkanMaterial::VulkanMaterial(const std::string& name, const MaterialTemplate& config)
        : IMaterial(name),
          m_Template(config)
    {
        m_UniformData.DiffuseColor = m_Template.diffuseColor;

        VA_ENGINE_DEBUG("[VulkanMaterial] Material '{}' created.", m_Name);
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
