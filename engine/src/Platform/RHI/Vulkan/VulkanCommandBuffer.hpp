//
// Created by Michael Desmedt on 19/05/2025.
//
#pragma once

#include <vulkan/vulkan.h>

namespace VoidArchitect::Platform
{
    enum class CommandBufferState
    {
        InRenderpass,
        Recording
    };

    class VulkanCommandBuffer
    {
    public:
        void SetState(const CommandBufferState& state) { m_State = state; }

        CommandBufferState GetState() const { return m_State; }
        VkCommandBuffer GetHandle() const { return m_CommandBuffer; }

    private:
        VkCommandBuffer m_CommandBuffer;
        CommandBufferState m_State;
    };
} // VoidArchitect
