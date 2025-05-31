//
// Created by Michael Desmedt on 31/05/2025.
//
#pragma once
#include "Core/Uuid.hpp"
#include "Resources/Material.hpp"

#include <vulkan/vulkan_core.h>

namespace VoidArchitect
{
    namespace Platform
    {
        class VulkanBuffer;

        class VulkanRHI;
        class VulkanDevice;

        struct MaterialSlot
        {
            bool isActive = false;
            UUID materialUUID = InvalidUUID;
            uint32_t generation = 0;
            bool needsUpdate = true;
        };

        class VulkanMaterialBufferManager
        {
        public:
            static constexpr size_t MAX_MATERIALS = 1024;

            VulkanMaterialBufferManager(
                VulkanRHI& rhi,
                const std::unique_ptr<VulkanDevice>& device,
                VkAllocationCallbacks* allocator);
            ~VulkanMaterialBufferManager() = default;

            uint32_t AllocateSlot(const UUID& materialUUID);
            void ReleaseSlot(uint32_t slotIndex);

            void UpdateMaterial(uint32_t slotIndex, const Resources::MaterialUniformObject& data);

            struct BindingInfo
            {
                VkBuffer buffer;
                uint32_t offset;
                uint32_t range;
            };

            BindingInfo GetBindingInfo(uint32_t slotIndex) const;

            void FlushUpdates();

        private:
            VulkanRHI& m_RHI;
            VkDevice m_Device;
            VkAllocationCallbacks* m_Allocator;

            std::unique_ptr<VulkanBuffer> m_MaterialUniformBuffer;

            std::vector<MaterialSlot> m_MaterialSlots;
            std::queue<uint32_t> m_FreeSlots;
            uint32_t m_NextFreeSlot = 0;

            std::vector<Resources::MaterialUniformObject> m_StagingData;
            bool m_NeedsUpdate = false;
        };

        inline std::unique_ptr<VulkanMaterialBufferManager> g_VkMaterialBufferManager;
    } // namespace Platform
} // namespace VoidArchitect
