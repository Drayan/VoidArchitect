//
// Created by Michael Desmedt on 18/05/2025.
//
#include "VulkanShader.hpp"

#include <VoidArchitect/Engine/Common/Logger.hpp>
#include <VoidArchitect/Engine/RHI/Resources/Shader.hpp>

#include "VulkanDevice.hpp"
#include "VulkanUtils.hpp"

namespace VoidArchitect::Platform
{
    VulkanShader::VulkanShader(
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        const std::string& name,
        const Resources::ShaderConfig& config,
        const VAArray<uint8_t>& shaderCode)
        : IShader(name, config.stage, config.entry),
          m_Device(device->GetLogicalDeviceHandle()),
          m_Allocator(allocator),
          m_ShaderModule{},
          m_ShaderStageInfo{}
    {
        auto createInfo = VkShaderModuleCreateInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = shaderCode.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkCreateShaderModule(m_Device, &createInfo, m_Allocator, &m_ShaderModule));

        m_ShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        switch (m_Stage)
        {
            case Resources::ShaderStage::Vertex:
                m_ShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
                break;
            case Resources::ShaderStage::Pixel:
                m_ShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                break;
            default: VA_ENGINE_ERROR("[VulkanShader] Unsupported shader stage.");
                break;
        }
        m_ShaderStageInfo.module = m_ShaderModule;
        m_ShaderStageInfo.pName = m_EntryPoint.c_str();

        VA_ENGINE_TRACE("[VulkanShader] Shader module created.");
    }

    VulkanShader::~VulkanShader()
    {
        if (m_ShaderModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(m_Device, m_ShaderModule, m_Allocator);
            VA_ENGINE_TRACE("[VulkanShader] Shader module destroyed.");
        }
    }

    VulkanShader::VulkanShader(VulkanShader&& other) noexcept
        : m_Device(other.m_Device),
          m_Allocator(other.m_Allocator),
          m_ShaderModule(other.m_ShaderModule),
          m_ShaderStageInfo(other.m_ShaderStageInfo)

    {
        other.InvalidateResources();
        VA_ENGINE_TRACE("[VulkanShader] Shader module moved.");
    }

    VulkanShader& VulkanShader::operator=(VulkanShader&& other) noexcept
    {
        if (this != &other)
        {
            this->~VulkanShader();

            m_Device = other.m_Device;
            m_Allocator = other.m_Allocator;
            m_ShaderModule = other.m_ShaderModule;
            m_ShaderStageInfo = other.m_ShaderStageInfo;

            other.InvalidateResources();
            VA_ENGINE_TRACE("[VulkanShader] Shader module moved.");
        }

        return *this;
    }

    void VulkanShader::InvalidateResources()
    {
        m_Device = VK_NULL_HANDLE;
        m_Allocator = nullptr;
        m_ShaderModule = VK_NULL_HANDLE;
    }
} // namespace VoidArchitect::Platform
