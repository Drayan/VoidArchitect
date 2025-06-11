//
// Created by Michael Desmedt on 20/05/2025.
//
#pragma once

#include "Resources/Material.hpp"

#include <vulkan/vulkan.h>

#include "Systems/MaterialSystem.hpp"

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
        VulkanMaterial(
            const std::string& name,
            const MaterialTemplate& config);
        ~VulkanMaterial() override = default;

        void SetDiffuseColor(const Math::Vec4& color) override;
        void SetTexture(Resources::TextureUse use, Resources::TextureHandle texture) override;

        const MaterialTemplate& GetTemplate() const { return m_Template; }
        const Resources::MaterialUniformObject& GetUniformData() const { return m_UniformData; }
        Resources::TextureHandle GetTexture(Resources::TextureUse use) const;
        bool IsDirty() const { return m_IsDirty; }
        void ClearDirtyFlag() { m_IsDirty = false; }

    private:
        MaterialTemplate m_Template;

        Resources::MaterialUniformObject m_UniformData;
        //TODO: Replace this with handle when TextureSystem is refactored
        VAHashMap<Resources::TextureUse, Resources::TextureHandle> m_Textures;

        bool m_IsDirty = true;
    };
} // namespace VoidArchitect::Platform
