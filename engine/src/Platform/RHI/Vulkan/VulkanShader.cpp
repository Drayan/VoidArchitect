//
// Created by Michael Desmedt on 18/05/2025.
//
#include "VulkanShader.hpp"

#include "VulkanDevice.hpp"
#include "VulkanUtils.hpp"
#include "Core/Logger.hpp"

namespace VoidArchitect::Platform
{
    VulkanShader::VulkanShader(
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        const ShaderStage stage,
        const std::string& path)
        : m_Device(device->GetLogicalDeviceHandle()),
          m_Allocator(allocator),
          m_ShaderModule{},
          m_ShaderStageInfo{},
          m_Stage(stage)
    {
        const std::vector<char> shaderCode = readFromDisk(path);
        auto createInfo = VkShaderModuleCreateInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = shaderCode.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkCreateShaderModule(m_Device, &createInfo, m_Allocator, &m_ShaderModule));

        m_ShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        switch (m_Stage)
        {
            case ShaderStage::Vertex:
                m_ShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
                break;
            case ShaderStage::Pixel:
                m_ShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                break;
            default:
                VA_ENGINE_ERROR("[VulkanShader] Unsupported shader stage.");
                break;
        }
        m_ShaderStageInfo.module = m_ShaderModule;
        m_ShaderStageInfo.pName = "main";

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

    std::vector<char> VulkanShader::readFromDisk(const std::string& filename)
    {
        constexpr auto shaderDir = "assets/shaders/";
        constexpr auto shaderExtension = ".spv";
        const std::string shaderPath = shaderDir + filename + shaderExtension;
        std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            VA_ENGINE_ERROR("[VulkanShader] Failed to open file: {}", shaderPath);
            throw std::runtime_error("Failed to open file!");
        }

        const std::streamsize fileSize = file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }
} // VoidArchitect
