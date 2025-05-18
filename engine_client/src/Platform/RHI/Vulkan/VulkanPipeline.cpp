//
// Created by Michael Desmedt on 18/05/2025.
//
#include "VulkanPipeline.hpp"

#include "VulkanDevice.hpp"

namespace VoidArchitect::Platform
{
    VulkanPipeline::VulkanPipeline(
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator)
        : m_Device(device->GetLogicalDeviceHandle()),
          m_Allocator(allocator)
    {
        m_Shaders.emplace_back(device, allocator, "BuiltinObject.vert");
        m_Shaders.emplace_back(device, allocator, "BuiltinObject.pixl");
    }
} // VoidArchitect
