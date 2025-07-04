//
// Created by Michael Desmedt on 08/06/2025.
//
#include "VulkanExecutionContext.hpp"

#include "VulkanBuffer.hpp"
#include "VulkanCommandBuffer.hpp"
#include "VulkanDevice.hpp"
#include "VulkanFence.hpp"
#include "VulkanFramebufferCache.hpp"
#include "VulkanMesh.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanRenderPass.hpp"
#include "VulkanRenderTarget.hpp"
#include "VulkanRenderTargetSystem.hpp"
#include "VulkanSwapchain.hpp"
#include "VulkanUtils.hpp"
#include "Core/Logger.hpp"
#include "Resources/Material.hpp"
#include "Systems/MeshSystem.hpp"

namespace VoidArchitect::Platform
{
    VulkanExecutionContext::VulkanExecutionContext(
        const std::unique_ptr<VulkanDevice>& device,
        VkAllocationCallbacks* allocator,
        uint32_t width,
        uint32_t height)
        : m_Device(device),
          m_Allocator(allocator),
          m_CurrentWidth(width),
          m_CurrentHeight(height)
    {
        m_Swapchain = std::make_unique<VulkanSwapchain>(
            m_Device,
            m_Allocator,
            m_CurrentWidth,
            m_CurrentHeight);

        CreateCommandBuffers();
        CreateSyncObjects();
        CreateGlobalUBO();

        m_FramebufferCache = std::make_unique<VulkanFramebufferCache>(m_Device, m_Allocator);
    }

    VulkanExecutionContext::~VulkanExecutionContext()
    {
        m_FramebufferCache = nullptr;

        if (m_GlobalDescriptorSets != nullptr)
        {
            delete[] m_GlobalDescriptorSets;
            m_GlobalDescriptorSets = nullptr;
            VA_ENGINE_TRACE("[VulkanExecutionContext] Global descriptor sets destroyed.");
        }
        m_GlobalUniformBuffer = nullptr;
        VA_ENGINE_TRACE("[VulkanExecutionContext] Global uniform buffer destroyed.");

        if (m_GlobalDescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(
                m_Device->GetLogicalDeviceHandle(),
                m_GlobalDescriptorPool,
                m_Allocator);
            m_GlobalDescriptorPool = VK_NULL_HANDLE;
            VA_ENGINE_TRACE("[VulkanExecutionContext] Global descriptor pool destroyed.");
        }

        if (m_GlobalDescriptorSetLayout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(
                m_Device->GetLogicalDeviceHandle(),
                m_GlobalDescriptorSetLayout,
                m_Allocator);
            m_GlobalDescriptorSetLayout = VK_NULL_HANDLE;
            VA_ENGINE_TRACE("[VulkanExecutionContext] Global descriptor set layout destroyed.");
        }

        DestroySyncObjects();
    }

    bool VulkanExecutionContext::BeginFrame(float deltaTime)
    {
        const auto device = m_Device->GetLogicalDeviceHandle();
        if (m_RecreatingSwapchain)
        {
            if (const auto result = vkDeviceWaitIdle(device); result != VK_SUCCESS && result !=
                VK_TIMEOUT)
            {
                VA_ENGINE_ERROR("[VulkanRHI] Failed to wait for device idle.");
                return false;
            }
            VA_ENGINE_INFO("[VulkanRHI] Recreating swapchain.");
        }

        // Check if the window has been resized. If so, a new swapchain must be created.
        if (m_ProcessedSizeGeneration != m_RequestedSizeGeneration)
        {
            if (m_CurrentWidth == 0 || m_CurrentHeight == 0) return false;

            HandleResize();

            return false;
        }

        if (!m_InFlightFences[m_CurrentIndex].Wait())
        {
            VA_ENGINE_WARN("[VulkanRHI] Failed to wait for fence.");
            return false;
        }

        auto& acquisitionFence = m_ImageAcquisitionFences[m_CurrentIndex];
        if (!acquisitionFence.Wait())
        {
            VA_ENGINE_WARN("[VulkanRHI] Failed to wait for image acquisition fence.");
        }
        acquisitionFence.Reset();

        const auto index = m_Swapchain->AcquireNextImage(
            std::numeric_limits<uint64_t>::max(),
            m_ImageAvailableSemaphores[m_CurrentIndex],
            acquisitionFence.GetHandle());

        if (!index.has_value()) return false;
        m_ImageIndex = index.value();

        if (!acquisitionFence.Wait())
        {
            VA_ENGINE_ERROR("[VulkanExecutionContext] Failed to wait for image acquisition fence.");
            return false;
        }

        // Begin recording commands.
        auto& cmdBuf = m_GraphicsCommandBuffers[m_ImageIndex];
        cmdBuf.Reset();
        cmdBuf.Begin();

        const VkViewport viewport = {
            .x = 0.0f,
            .y = static_cast<float>(m_CurrentHeight),
            .width = static_cast<float>(m_CurrentWidth),
            .height = -static_cast<float>(m_CurrentHeight),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        const VkRect2D scissor = {.offset = {0, 0}, .extent = {m_CurrentWidth, m_CurrentHeight}};

        vkCmdSetViewport(cmdBuf.GetHandle(), 0, 1, &viewport);
        vkCmdSetScissor(cmdBuf.GetHandle(), 0, 1, &scissor);

        return true;
    }

    void VulkanExecutionContext::BeginRenderPass(
        const RenderPassHandle passHandle,
        const VAArray<Resources::RenderTargetHandle>& targetHandles)
    {
        const auto& passConfig = g_RenderPassSystem->GetConfigFor(passHandle);
        const auto sortedAttachments = SortAttachmentsToMatchRenderPassOrder(
            passConfig,
            targetHandles);

        const auto vulkanPass = dynamic_cast<VulkanRenderPass*>(g_RenderPassSystem->GetPointerFor(
            passHandle));

        const auto framebuffer = m_FramebufferCache->GetHandleFor(
            vulkanPass->GetHandle(),
            sortedAttachments,
            m_CurrentWidth,
            m_CurrentHeight);

        VkRenderPassBeginInfo renderPassBeginInfo{};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = vulkanPass->m_Renderpass;
        renderPassBeginInfo.framebuffer = framebuffer;
        renderPassBeginInfo.renderArea.offset = {0, 0};
        renderPassBeginInfo.renderArea.extent = {m_CurrentWidth, m_CurrentHeight};
        renderPassBeginInfo.clearValueCount = vulkanPass->m_ClearValues.size();
        renderPassBeginInfo.pClearValues = vulkanPass->m_ClearValues.data();

        vkCmdBeginRenderPass(
            m_GraphicsCommandBuffers[m_ImageIndex].GetHandle(),
            &renderPassBeginInfo,
            VK_SUBPASS_CONTENTS_INLINE);

        m_GraphicsCommandBuffers[m_ImageIndex].SetState(CommandBufferState::InRenderpass);
    }

    void VulkanExecutionContext::EndRenderPass() const
    {
        vkCmdEndRenderPass(m_GraphicsCommandBuffers[m_ImageIndex].GetHandle());
    }

    bool VulkanExecutionContext::EndFrame(float deltaTime)
    {
        auto& cmdBuf = m_GraphicsCommandBuffers[m_ImageIndex];

        cmdBuf.End();

        if (m_ImagesInFlight[m_ImageIndex] != nullptr) m_ImagesInFlight[m_ImageIndex]->Wait(
            std::numeric_limits<uint64_t>::max());

        // Mark the image fence as in use by this frame.
        m_ImagesInFlight[m_ImageIndex] = &m_InFlightFences[m_CurrentIndex];

        // Reset the fence for use in the next frame.
        m_InFlightFences[m_CurrentIndex].Reset();

        const auto cmdBufHandle = cmdBuf.GetHandle();
        auto submitInfo = VkSubmitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBufHandle;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_QueueCompleteSemaphores[m_CurrentIndex];
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_ImageAvailableSemaphores[m_CurrentIndex];

        const VAArray flags = {VkPipelineStageFlags{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}};
        submitInfo.pWaitDstStageMask = flags.data();

        if (VA_VULKAN_CHECK_RESULT(
            vkQueueSubmit( m_Device->GetGraphicsQueueHandle(), 1, &submitInfo, m_InFlightFences[
                m_CurrentIndex].GetHandle())))
        {
            VA_ENGINE_ERROR("[VulkanRHI] Failed to submit graphics command buffer.");
            return false;
        }
        cmdBuf.SetState(CommandBufferState::Submitted);

        m_Swapchain->Present(
            m_Device->GetGraphicsQueueHandle(),
            m_QueueCompleteSemaphores[m_CurrentIndex],
            m_ImageIndex);

        m_CurrentIndex = (m_CurrentIndex + 1) % m_Swapchain->GetMaxFrameInFlight();

        return true;
    }

    void VulkanExecutionContext::UpdateGlobalState(const Resources::GlobalUniformObject& gUBO) const
    {
        const auto& deviceProps = m_Device->GetProperties();
        const auto minAlign = deviceProps.limits.minUniformBufferOffsetAlignment;

        constexpr auto uniformObjectSize = sizeof(Resources::GlobalUniformObject);
        const size_t alignedSize = AlignUp(uniformObjectSize, minAlign);
        VAArray<uint8_t> alignedData(alignedSize * 3);

        // Pad the data if it's not aligned.
        for (uint32_t i = 0; i < 3; i++)
        {
            std::memcpy(alignedData.data() + i * alignedSize, &gUBO, uniformObjectSize);
            // Remaining is padded with zeroes.
        }
        m_GlobalUniformBuffer->LoadData(alignedData);

        // Update the descriptor set for the current frame
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_GlobalUniformBuffer->GetHandle();
        bufferInfo.offset = alignedSize * m_ImageIndex;
        bufferInfo.range = uniformObjectSize;

        VkWriteDescriptorSet writeDescriptorSet{};
        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.dstSet = m_GlobalDescriptorSets[m_ImageIndex];
        writeDescriptorSet.dstBinding = 0;
        writeDescriptorSet.dstArrayElement = 0;
        writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(
            m_Device->GetLogicalDeviceHandle(),
            1,
            &writeDescriptorSet,
            0,
            nullptr);
    }

    void VulkanExecutionContext::BindRenderState(RenderStateHandle stateHandle)
    {
        const auto& cmdBuf = GetCurrentCommandBuffer();
        const auto* vkRenderState = dynamic_cast<VulkanPipeline*>(g_RenderStateSystem->
            GetPointerFor(stateHandle));
        vkCmdBindPipeline(
            cmdBuf.GetHandle(),
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            vkRenderState->GetHandle());

        vkCmdBindDescriptorSets(
            cmdBuf.GetHandle(),
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            vkRenderState->GetPipelineLayout(),
            0,
            1,
            &m_GlobalDescriptorSets[m_ImageIndex],
            0,
            nullptr);

        m_LastBoundPipelineLayout = vkRenderState->GetPipelineLayout();
    }

    void VulkanExecutionContext::BindMaterialGroup(
        const MaterialHandle materialHandle,
        const RenderStateHandle stateHandle)
    {
        const auto& cmdBuf = GetCurrentCommandBuffer();

        g_VkBindingGroupManager->BindMaterialGroup(cmdBuf.GetHandle(), materialHandle, stateHandle);
    }

    void VulkanExecutionContext::BindMesh(const Resources::MeshHandle meshHandle)
    {
        const auto& cmdBuf = GetCurrentCommandBuffer();
        auto* vkMesh = dynamic_cast<VulkanMesh*>(g_MeshSystem->GetPointerFor(meshHandle));
        const auto* vkVertices = dynamic_cast<VulkanVertexBuffer*>(vkMesh->GetVertexBuffer());
        const auto* vkIndices = dynamic_cast<VulkanIndexBuffer*>(vkMesh->GetIndexBuffer());

        const VkDeviceSize offsets = {0};
        const auto vertBuf = vkVertices->GetHandle();
        vkCmdBindVertexBuffers(cmdBuf.GetHandle(), 0, 1, &vertBuf, &offsets);
        vkCmdBindIndexBuffer(cmdBuf.GetHandle(), vkIndices->GetHandle(), 0, VK_INDEX_TYPE_UINT32);
    }

    void VulkanExecutionContext::PushConstants(
        const Resources::ShaderStage stage,
        const uint32_t size,
        const void* data)
    {
        const auto& cmdBuf = GetCurrentCommandBuffer();
        vkCmdPushConstants(
            cmdBuf.GetHandle(),
            m_LastBoundPipelineLayout,
            TranslateEngineShaderStageToVulkan(stage),
            0,
            size,
            data);
    }

    void VulkanExecutionContext::DrawIndexed(
        const uint32_t indexCount,
        const uint32_t indexOffset,
        const uint32_t vertexOffset,
        const uint32_t instanceCount,
        const uint32_t firstInstance)
    {
        const auto& cmdBuf = GetCurrentCommandBuffer();
        VA_ENGINE_ASSERT(cmdBuf.GetHandle() != VK_NULL_HANDLE, "No current command buffer");

        vkCmdDrawIndexed(
            cmdBuf.GetHandle(),
            indexCount,
            instanceCount,
            indexOffset,
            vertexOffset,
            firstInstance);
    }

    void VulkanExecutionContext::RequestResize(const uint32_t width, const uint32_t height)
    {
        m_PendingWidth = width;
        m_PendingHeight = height;
        m_RequestedSizeGeneration++;

        VA_ENGINE_DEBUG(
            "[VulkanExecutionContext] Resizing to {}x{}, generation : {}",
            width,
            height,
            m_RequestedSizeGeneration);
    }

    void VulkanExecutionContext::HandleResize()
    {
        m_RecreatingSwapchain = true;
        m_Device->WaitIdle();

        // Clear the ImageInFlight
        m_ImagesInFlight.clear();
        m_ImagesInFlight.resize(m_Swapchain->GetImageCount());

        m_Swapchain->Recreate(m_PendingWidth, m_PendingHeight);
        m_FramebufferCache->Clear();
        VA_ENGINE_INFO("[VulkanRHI] Recreating swapchain.");

        // Sync
        m_CurrentWidth = m_PendingWidth;
        m_CurrentHeight = m_PendingHeight;
        m_PendingWidth = 0;
        m_PendingHeight = 0;

        m_ProcessedSizeGeneration = m_RequestedSizeGeneration;
        m_RecreatingSwapchain = false;
    }

    void VulkanExecutionContext::CreateCommandBuffers()
    {
        m_GraphicsCommandBuffers.clear();

        const auto imageCount = m_Swapchain->GetImageCount();
        m_GraphicsCommandBuffers.reserve(imageCount);
        for (uint32_t i = 0; i < imageCount; i++)
        {
            m_GraphicsCommandBuffers.emplace_back(m_Device, m_Device->GetGraphicsCommandPool());
        }

        VA_ENGINE_INFO("[VulkanRHI] Command buffers created.");
    }

    void VulkanExecutionContext::CreateSyncObjects()
    {
        const auto maxFrameInFlight = m_Swapchain->GetMaxFrameInFlight();
        m_ImageAvailableSemaphores.reserve(maxFrameInFlight);
        m_QueueCompleteSemaphores.reserve(maxFrameInFlight);
        m_InFlightFences.reserve(maxFrameInFlight);

        for (uint32_t i = 0; i < maxFrameInFlight; i++)
        {
            auto semaphoreCreateInfo = VkSemaphoreCreateInfo{};
            semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            VkSemaphore semaphore;
            VA_VULKAN_CHECK_RESULT_WARN(
                vkCreateSemaphore( m_Device->GetLogicalDeviceHandle(), &semaphoreCreateInfo,
                    m_Allocator, & semaphore));
            m_ImageAvailableSemaphores.push_back(semaphore);

            VA_VULKAN_CHECK_RESULT_WARN(
                vkCreateSemaphore( m_Device->GetLogicalDeviceHandle(), &semaphoreCreateInfo,
                    m_Allocator, & semaphore));
            m_QueueCompleteSemaphores.push_back(semaphore);

            m_InFlightFences.emplace_back(m_Device, m_Allocator, true);
        }

        m_ImagesInFlight.resize(m_Swapchain->GetImageCount());
        for (uint32_t i = 0; i < m_Swapchain->GetImageCount(); i++)
        {
            m_ImagesInFlight.emplace_back(nullptr);
        }

        m_ImageAcquisitionFences.reserve(maxFrameInFlight);
        for (uint32_t i = 0; i < maxFrameInFlight; i++)
        {
            m_ImageAcquisitionFences.emplace_back(m_Device, m_Allocator, true);
        }

        VA_ENGINE_INFO("[VulkanRHI] Sync objects created.");
    }

    void VulkanExecutionContext::CreateGlobalUBO()
    {
        // Global DescriptorSetLayout
        VkDescriptorSetLayoutBinding globalDescriptorSetLayoutBinding{};
        globalDescriptorSetLayoutBinding.binding = 0;
        globalDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        globalDescriptorSetLayoutBinding.descriptorCount = 1;
        globalDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
            VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo globalDescriptorSetLayoutInfo{};
        globalDescriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        globalDescriptorSetLayoutInfo.bindingCount = 1;
        globalDescriptorSetLayoutInfo.pBindings = &globalDescriptorSetLayoutBinding;

        VA_VULKAN_CHECK_RESULT_CRITICAL(
            vkCreateDescriptorSetLayout(m_Device->GetLogicalDeviceHandle(), &
                globalDescriptorSetLayoutInfo, m_Allocator, &m_GlobalDescriptorSetLayout));
        // --- Global descriptor pool ---
        auto globalPoolSize = VkDescriptorPoolSize{};
        globalPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        globalPoolSize.descriptorCount = m_Swapchain->GetImageCount();

        auto globalPoolInfo = VkDescriptorPoolCreateInfo{};
        globalPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        globalPoolInfo.maxSets = m_Swapchain->GetImageCount();
        globalPoolInfo.poolSizeCount = 1;
        globalPoolInfo.pPoolSizes = &globalPoolSize;

        VA_VULKAN_CHECK_RESULT_WARN(
            vkCreateDescriptorPool( m_Device->GetLogicalDeviceHandle(), &globalPoolInfo, m_Allocator
                , &m_GlobalDescriptorPool));

        const auto& deviceProps = m_Device->GetProperties();
        const auto minAlign = deviceProps.limits.minUniformBufferOffsetAlignment;

        constexpr auto uniformObjectSize = sizeof(Resources::GlobalUniformObject);
        const size_t alignedSize = AlignUp(uniformObjectSize, minAlign);

        m_GlobalUniformBuffer = std::make_unique<VulkanBuffer>(
            m_Device,
            m_Allocator,
            alignedSize * 3,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        const VAArray globalLayouts = {
            m_GlobalDescriptorSetLayout,
            m_GlobalDescriptorSetLayout,
            m_GlobalDescriptorSetLayout
        };

        m_GlobalDescriptorSets = new VkDescriptorSet[globalLayouts.size()];

        auto allocInfo = VkDescriptorSetAllocateInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_GlobalDescriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(globalLayouts.size());
        allocInfo.pSetLayouts = globalLayouts.data();
        VA_VULKAN_CHECK_RESULT_WARN(
            vkAllocateDescriptorSets( m_Device->GetLogicalDeviceHandle(), &allocInfo,
                m_GlobalDescriptorSets));
    }

    void VulkanExecutionContext::DestroySyncObjects()
    {
        for (const auto& semaphore : m_ImageAvailableSemaphores)
        {
            vkDestroySemaphore(m_Device->GetLogicalDeviceHandle(), semaphore, m_Allocator);
        }
        for (const auto& semaphore : m_QueueCompleteSemaphores)
        {
            vkDestroySemaphore(m_Device->GetLogicalDeviceHandle(), semaphore, m_Allocator);
        }
        m_ImageAvailableSemaphores.clear();
        m_QueueCompleteSemaphores.clear();
        m_InFlightFences.clear();
        m_ImagesInFlight.clear();
        m_ImageAcquisitionFences.clear();
    }

    VAArray<VkImageView> VulkanExecutionContext::SortAttachmentsToMatchRenderPassOrder(
        const Renderer::RenderPassConfig& config,
        const VAArray<Resources::RenderTargetHandle>& targetHandles) const
    {
        VAArray<VkImageView> sortedAttachments;
        sortedAttachments.reserve(config.attachments.size());

        // Build descriptors for expected attachments in config order
        VAArray<AttachmentDescriptor> expectedAttachments;
        expectedAttachments.reserve(config.attachments.size());

        for (uint32_t i = 0; i < config.attachments.size(); i++)
        {
            const auto& attachmentConfig = config.attachments[i];

            AttachmentDescriptor descriptor{};
            descriptor.name = attachmentConfig.name;
            descriptor.resolvedFormat = ResolveAttachmentFormat(attachmentConfig.format);
            descriptor.isDepthAttachment = (attachmentConfig.name == "depth" || attachmentConfig.
                format == Renderer::TextureFormat::SWAPCHAIN_DEPTH || attachmentConfig.format ==
                Renderer::TextureFormat::D32_SFLOAT || attachmentConfig.format ==
                Renderer::TextureFormat::D24_UNORM_S8_UINT);
            descriptor.configIndex = i;

            expectedAttachments.push_back(descriptor);
        }

        // For each expected attachment (in config order), find matching RenderTarget
        for (const auto& expectedAttachment : expectedAttachments)
        {
            bool found = false;

            for (const auto& targetHandle : targetHandles)
            {
                const auto renderTarget = g_VkRenderTargetSystem->GetPointerFor(targetHandle);
                if (!renderTarget)
                {
                    VA_ENGINE_CRITICAL(
                        "[VulkanExecutionContext] Invalid RenderTarget handle during attachment sorting.");
                    throw std::runtime_error(
                        "Invalid RenderTarget handle during attachment sorting.");
                }

                // Try matching by name first
                if (renderTarget->GetName() == expectedAttachment.name)
                {
                    ValidateAttachmentCompatibility(expectedAttachment, renderTarget);

                    const auto vkTarget = dynamic_cast<VulkanRenderTarget*>(renderTarget);
                    sortedAttachments.push_back(vkTarget->GetImageView());
                    found = true;
                    break;
                }
            }

            // If name matching failed, try format + type matching
            if (!found)
            {
                for (const auto& targetHandle : targetHandles)
                {
                    const auto renderTarget = g_VkRenderTargetSystem->GetPointerFor(targetHandle);
                    const VkFormat targetFormat = TranslateEngineTextureFormatToVulkan(
                        renderTarget->GetFormat());

                    // Check format compatibility and depth/color type
                    bool formatMatches = (targetFormat == expectedAttachment.resolvedFormat);
                    bool typeMatches = expectedAttachment.isDepthAttachment
                        ? renderTarget->IsDepth()
                        : renderTarget->IsColor();

                    if (formatMatches && typeMatches)
                    {
                        ValidateAttachmentCompatibility(expectedAttachment, renderTarget);

                        const auto vkTarget = dynamic_cast<VulkanRenderTarget*>(renderTarget);
                        sortedAttachments.push_back(vkTarget->GetImageView());
                        found = true;
                        break;
                    }
                }
            }

            // Critical error if attachment not found at this point.
            if (!found)
            {
                VA_ENGINE_CRITICAL(
                    "[VulkanExecutionContext] Required attachment '{}' (format: {}, depth: {}) not found in RenderTarget list.",
                    expectedAttachment.name,
                    static_cast<int>(expectedAttachment.resolvedFormat),
                    expectedAttachment.isDepthAttachment);
                throw std::runtime_error(
                    "Attachment " + expectedAttachment.name + " not found in RenderTarget list.");
            }
        }

        // Ensure we have the expected number of attachments
        if (sortedAttachments.size() != config.attachments.size())
        {
            VA_ENGINE_CRITICAL(
                "[VulkanExecutionContext] Attachment count mismatch: expected {}, got {}.",
                config.attachments.size(),
                sortedAttachments.size());
            throw std::runtime_error(
                "Expected " + std::to_string(config.attachments.size()) + " attachments, got " +
                std::to_string(sortedAttachments.size()) + ".");
        }

        return sortedAttachments;
    }

    VkFormat VulkanExecutionContext::ResolveAttachmentFormat(
        Renderer::TextureFormat engineFormat) const
    {
        switch (engineFormat)
        {
            case Renderer::TextureFormat::SWAPCHAIN_FORMAT:
                return m_Swapchain->GetFormat();
            case Renderer::TextureFormat::SWAPCHAIN_DEPTH:
                return m_Swapchain->GetDepthFormat();
            default:
                return TranslateEngineTextureFormatToVulkan(engineFormat);
        }
    }

    void VulkanExecutionContext::ValidateAttachmentCompatibility(
        const AttachmentDescriptor& expected,
        const Resources::IRenderTarget* renderTarget) const
    {
        const VkFormat targetFormat = TranslateEngineTextureFormatToVulkan(
            renderTarget->GetFormat());

        if (targetFormat != expected.resolvedFormat)
        {
            VA_ENGINE_WARN(
                "[VulkanExecutionContext] Format mismatch for attachment '{}': expected {}, got {}.",
                expected.name,
                static_cast<int>(expected.resolvedFormat),
                static_cast<int>(targetFormat));
        }

        bool isDepthTarget = renderTarget->IsDepth();
        if (isDepthTarget != expected.isDepthAttachment)
        {
            VA_ENGINE_WARN(
                "[VulkanExecutionContext] Depth/color mismatch for attachment '{}': expected {}, got {}.",
                expected.name,
                expected.isDepthAttachment,
                isDepthTarget);
        }
    }

    Resources::RenderTargetHandle VulkanExecutionContext::GetCurrentColorRenderTargetHandle() const
    {
        return m_Swapchain->GetColorRenderTarget(m_ImageIndex);
    }

    Resources::RenderTargetHandle VulkanExecutionContext::GetDepthRenderTargetHandle() const
    {
        return m_Swapchain->GetDepthRenderTarget();
    }

    VkFormat VulkanExecutionContext::GetSwapchainFormat() const
    {
        return m_Swapchain->GetFormat();
    }

    VkFormat VulkanExecutionContext::GetDepthFormat() const
    {
        return m_Swapchain->GetDepthFormat();
    }
}
