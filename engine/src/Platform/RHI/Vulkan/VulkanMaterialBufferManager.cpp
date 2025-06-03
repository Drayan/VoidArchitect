//
// Created by Michael Desmedt on 31/05/2025.
//
#include "VulkanMaterialBufferManager.hpp"

#include "Core/Logger.hpp"
#include "VulkanBuffer.hpp"
#include "VulkanDevice.hpp"
#include "VulkanRhi.hpp"

namespace VoidArchitect
{
    namespace Platform
    {
        VulkanMaterialBufferManager::VulkanMaterialBufferManager(
            VulkanRHI& rhi,
            const std::unique_ptr<VulkanDevice>& device,
            VkAllocationCallbacks* allocator)
            : m_RHI(rhi),
              m_Device(device->GetLogicalDeviceHandle()),
              m_Allocator(allocator)
        {
            const auto bufferSize = sizeof(Resources::MaterialUniformObject) * MAX_MATERIALS;

            m_MaterialUniformBuffer = std::make_unique<VulkanBuffer>(
                rhi,
                rhi.GetDeviceRef(),
                m_Allocator,
                bufferSize,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            m_MaterialSlots.resize(MAX_MATERIALS);
            m_StagingData.resize(MAX_MATERIALS);

            VA_ENGINE_INFO(
                "[VulkanMaterialBufferManager] Initialized with {} slots.", MAX_MATERIALS);
        }

        uint32_t VulkanMaterialBufferManager::AllocateSlot(const UUID& materialUUID)
        {
            uint32_t slotIndex;
            if (!m_FreeSlots.empty())
            {
                slotIndex = m_FreeSlots.front();
                m_FreeSlots.pop();
            }
            else if (m_NextFreeSlot < MAX_MATERIALS)
            {
                slotIndex = m_NextFreeSlot;
                ++m_NextFreeSlot;
            }
            else
            {
                VA_ENGINE_ERROR("[VulkanMaterialBufferManager] No more material slots available!");
                return std::numeric_limits<uint32_t>::max();
            }

            auto& slot = m_MaterialSlots[slotIndex];
            slot.isActive = true;
            slot.materialUUID = materialUUID;
            slot.needsUpdate = true;

            VA_ENGINE_TRACE(
                "[VulkanMaterialBufferManager] Allocated slot {} for material '{}’.",
                slotIndex,
                static_cast<uint64_t>(materialUUID));

            return slotIndex;
        }

        void VulkanMaterialBufferManager::ReleaseSlot(uint32_t slotIndex)
        {
            if (slotIndex >= MAX_MATERIALS || !m_MaterialSlots[slotIndex].isActive)
            {
                VA_ENGINE_WARN(
                    "[VulkanMaterialBufferManager] Attempting to release an invalid slot index {}.",
                    slotIndex);
                return;
            }

            auto& slot = m_MaterialSlots[slotIndex];
            slot.isActive = false;
            slot.materialUUID = InvalidUUID;
            slot.needsUpdate = false;

            m_FreeSlots.push(slotIndex);

            VA_ENGINE_TRACE("[VulkanMaterialBufferManager] Released slot {}.", slotIndex);
        }

        void VulkanMaterialBufferManager::UpdateMaterial(
            uint32_t slotIndex, const Resources::MaterialUniformObject& data)
        {
            if (slotIndex >= MAX_MATERIALS || !m_MaterialSlots[slotIndex].isActive)
            {
                VA_ENGINE_WARN(
                    "[VulkanMaterialBufferManager] Attempting to update an invalid slot index {}.",
                    slotIndex);
                return;
            }

            m_StagingData[slotIndex] = data;
            m_MaterialSlots[slotIndex].needsUpdate = true;
            m_NeedsUpdate = true;
        }

        VulkanMaterialBufferManager::BindingInfo VulkanMaterialBufferManager::GetBindingInfo(
            uint32_t slotIndex) const
        {
            if (slotIndex >= MAX_MATERIALS || !m_MaterialSlots[slotIndex].isActive)
            {
                VA_ENGINE_WARN(
                    "[VulkanMaterialBufferManager] Attempting to get binding info for an invalid "
                    "slot index {}.",
                    slotIndex);
            }

            return {
                m_MaterialUniformBuffer->GetHandle(),
                slotIndex * static_cast<uint32_t>(sizeof(Resources::MaterialUniformObject)),
                sizeof(Resources::MaterialUniformObject)};
        }

        void VulkanMaterialBufferManager::FlushUpdates()
        {
            if (!m_NeedsUpdate)
                return;

            VA_ENGINE_TRACE("[VulkanMaterialBufferManager] Flushing updates.");
            m_MaterialUniformBuffer->LoadData(m_StagingData);

            // Mark all the slots as up to date.
            for (auto& slot : m_MaterialSlots)
                slot.needsUpdate = false;

            m_NeedsUpdate = false;
        }
    } // namespace Platform
} // namespace VoidArchitect
