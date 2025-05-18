//
// Created by Michael Desmedt on 18/05/2025.
//
#pragma once
#include <vulkan/vulkan.h>

#include "VulkanShader.hpp"

namespace VoidArchitect::Platform
{
    class VulkanDevice;

    class VulkanPipeline
    {
    public:
        VulkanPipeline(
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator);

    private:
        VkDevice m_Device;
        VkAllocationCallbacks* m_Allocator;

        std::vector<VulkanShader> m_Shaders;
    };
} // VoidArchitect
