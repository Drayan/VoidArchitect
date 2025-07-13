//
// Created by Michael Desmedt on 18/05/2025.
//
#pragma once
#include "Resources/Shader.hpp"

#include <vulkan/vulkan.h>

namespace VoidArchitect::Platform
{
    class VulkanDevice;

    class VulkanShader : public Resources::IShader
    {
    public:
        VulkanShader(
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            const std::string& name,
            const Resources::ShaderConfig& config,
            const VAArray<uint8_t>& shaderCode);
        VulkanShader(VulkanShader&& other) noexcept;
        VulkanShader(const VulkanShader& other) = delete;
        ~VulkanShader() override;

        VulkanShader& operator=(VulkanShader&& other) noexcept;
        VulkanShader& operator=(const VulkanShader& other) = delete;

        VkPipelineShaderStageCreateInfo GetShaderStageInfo() const { return m_ShaderStageInfo; }

    private:
        void InvalidateResources();

        VkDevice m_Device;
        VkAllocationCallbacks* m_Allocator;

        VkShaderModule m_ShaderModule;
        VkPipelineShaderStageCreateInfo m_ShaderStageInfo;
    };
} // namespace VoidArchitect::Platform
