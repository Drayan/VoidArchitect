//
// Created by Michael Desmedt on 18/05/2025.
//
#pragma once
#include <vulkan/vulkan.h>

namespace VoidArchitect::Platform
{
    class VulkanDevice;

    class VulkanShader
    {
    public:
        VulkanShader(
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator,
            const std::string& path);
        VulkanShader(VulkanShader&& other) noexcept;
        VulkanShader(const VulkanShader& other) = delete;
        ~VulkanShader();

        VulkanShader& operator=(VulkanShader&& other) noexcept;
        VulkanShader& operator=(const VulkanShader& other) = delete;

    private:
        static std::vector<char> readFromDisk(const std::string& filename);

        void InvalidateResources();

        std::string m_Path;
        VkDevice m_Device;
        VkAllocationCallbacks* m_Allocator;
        VkShaderModule m_ShaderModule;
    };
} // VoidArchitect
