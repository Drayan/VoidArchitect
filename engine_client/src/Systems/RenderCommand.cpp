//
// Created by Michael Desmedt on 14/05/2025.
//
#include "RenderCommand.hpp"

#include "Platform/RHI/IRenderingHardware.hpp"
#include "Platform/RHI/Vulkan/VulkanRhi.hpp"

namespace VoidArchitect
{
    Platform::IRenderingHardware* RenderCommand::m_RenderingHardware = nullptr;

    void RenderCommand::Initialize(Platform::RHI_API_TYPE apiType, std::unique_ptr<Window>& window)
    {
        switch (apiType)
        {
        case Platform::RHI_API_TYPE::Vulkan:
            m_RenderingHardware = new Platform::VulkanRHI(window);
            break;
        default:
            break;
        }
    }

    void RenderCommand::Shutdown()
    {
        delete m_RenderingHardware;
    }
} // VoidArchitect
