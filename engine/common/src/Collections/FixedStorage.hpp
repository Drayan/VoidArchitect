//
// Created by Michael Desmedt on 17/06/2025.
//
#pragma once
#include "VoidArchitect/Engine/Common/Handle.hpp"
#include "VoidArchitect/Engine/Common/Logger.hpp"

namespace VoidArchitect::Collections
{
    /// @brief Thread-safe fixed-capacity storage with handle-based access
    ///
    /// FixedStorage provides a thread-safe, fixed-capacity container that allocates
    /// objects in-place and provides access through generation-validated handles.
    /// This prevents ABA problems and provides stable object references.
    ///
    /// Key features:
    /// - Fixed capacity determined at compile time
    /// - Thread-safe allocation and deallocation using atomic operations
    /// - In-place construction with perfect forwarding
    /// - Generation-based handles prevent use-after-free bugs
    /// - Lock-free operations for high performance
    /// - Automatic object destruction on release
    ///
    /// @tparam T Type of objects to store (must be constructible and destructible)
    /// @tparam CAPACITY Maximum number of objects that can be stored simultaneously
    ///
    /// Usage example:
    /// @code
    /// // Define storage for game entities
    /// FixedStorage<Entity, 10000> entityStorage;
    ///
    /// // Allocate new entity with constructor parameters
    /// auto handle = entityStorage.Allocate(Vec3::Zero(), "Player");
    /// if (handle.IsValid())
    /// {
    ///     Entity* entity = entityStorage.Get(handle);
    ///     // Use entity safely...
    ///
    ///     // Release when done
    ///     entityStorage.Release(handle);
    /// }
    ///
    /// // Handle becomes invalid after release
    /// assert(!entityStorage.IsValid(handle));
    /// @endcode
    template <typename T, size_t CAPACITY>
    class FixedStorage
    {
    public:
        /// @brief Handle type for accessing stored objects
        using HandleType = Handle<T>;

        /// @brief Maximum number of objects that can be stored
        static constexpr size_t MAX_OBJECTS = CAPACITY;

        // === Constructors / Destructors ===

        /// @brief Default constructor
        ///
        /// Initializes empty storage with all slots available for allocation.
        FixedStorage();

        /// @brief Destructor
        ///
        /// Automatically releases all allocated objects and calls their destructors.
        ///
        /// @warning Objects should be manually released before destruction to ensure
        /// proper cleanup order in complex systems...
        ~FixedStorage();

        // Non-copyable and non-movable (contains atomic members)
        FixedStorage(const FixedStorage&) = delete;
        FixedStorage& operator=(const FixedStorage&) = delete;
        FixedStorage(FixedStorage&&) = delete;
        FixedStorage& operator=(FixedStorage&&) = delete;

        // === Allocation / Deallocation ===

        /// @brief Allocate a new slot and construct object in-place
        /// @tparam Args Constructor argument types (deduced)
        /// @param args Arguments to forward to T's constructor
        /// @return Valid handle on success, invalid otherwise
        ///
        /// This method is thread-safe and uses atomic operations to claim slots.
        /// The object is constructed in-place using perfect forwarding.
        ///
        /// @note If allocation fails due to full storage, a warning is logged
        /// and an invalid handle is returned.
        template <typename... Args>
        HandleType Allocate(Args&&... args);

        /// @brief Release a slot and destruct the object
        /// @param handle Handle to the object to release
        /// @return true if the object was successfully released, false if the handle was invalid
        ///
        /// This method is thread-safe and validates the handle before releasing.
        /// The object's destructor is called automatically.
        ///
        /// @warning Using the handle after release will result in undefined behavior.
        /// The handle becomes invalid immediately after this call.
        bool Release(HandleType handle);

        // === Access ===

        /// @brief Get mutable object by handle
        /// @param handle Handle to the object
        /// @return Pointer to object if handle is valid, nullptr otherwise
        ///
        /// This method is thread-safe for access but the returned object
        /// is not protected from concurrent modification. External synchronization
        /// may be required for thread-safe object access.
        T* Get(HandleType handle);

        /// @brief Get immutable object by handle
        /// @param handle Handle to the object
        /// @return Const pointer to object if handle is valid, nullptr otherwise
        const T* Get(HandleType handle) const;

        /// @brief Check if handle references a valid object
        /// @param handle Handle to validate
        /// @return true if handle is valid and references an allocated object
        ///
        /// This method performs full validation including generation checking
        /// to prevent use of stale handles.
        bool IsValid(HandleType handle) const;

        // === Statistics ===

        /// @brief Get the number of currently allocated slots
        /// @return Number of objects currently stored
        size_t GetUsedSlots() const { return m_UsedCount.load(std::memory_order_relaxed); }

        /// @brief Get maximum capacity of storage
        /// @return Maximum number of objects that can be stored
        constexpr size_t GetCapacity() const { return CAPACITY; }

        /// @brief Get the number of available slots
        /// @return Number of slots available for allocation
        size_t GetAvailableSlots() const { return CAPACITY - GetUsedSlots(); }

        /// @brief Check if storage is full
        /// @return true if no slots are available for allocation
        bool IsFull() const { return GetUsedSlots() >= CAPACITY; }

        /// @brief Check if storage is empty
        /// @return true if no objects are currently allocated
        bool IsEmpty() const { return GetUsedSlots() == 0; }

        /// @brief Get usage percentage
        /// @return Percentage of capacity currently used (0.0 to 1.0)
        float GetUsagePercentage() const
        {
            return static_cast<float>(GetUsedSlots()) / static_cast<float>(CAPACITY);
        }

        /// @brief Check if a specific slot is in use
        /// @param index Slot index to check
        /// @return true if the slot is used, false otherwise.
        bool IsUsed(size_t index)
        {
            if (index >= CAPACITY) return false;
            return m_Slots[index].inUse.load(std::memory_order_acquire);
        }

        /// @brief Array-like access to stored objects
        /// @param index Slot index to access
        /// @return Reference to object at the specified slot
        ///
        /// @warning Only call this on slots where IsUsed(index) returns true
        T& operator[](size_t index)
        {
            return *m_Slots[index].GetObject();
        }

        /// @brief Array-life access to stored objects (const version)
        /// @param index Slot index to access
        /// @return Const reference to object at the specified slot
        ///
        /// @warning Only call this on slots where IsUsed(index) returns true
        const T& operator[](size_t index) const
        {
            return *m_Slots[index].GetObject();
        }

        /// @brief Get a valid handle for a specific slot index
        /// @param index Slot index to get handle for
        /// @return Handle for this slot, or invalid handle if slot not in use
        ///
        /// This method allows advanced users to create handles for specific slots
        /// during controlled iteration or eviction scenarios. The returned handle
        /// is guaranteed to be valid for the current state of the slot.
        ///
        /// @note Returns invalid handle if slot is not in use
        HandleType GetHandleForSlot(size_t index) const
        {
            if (index >= CAPACITY) return HandleType::Invalid();

            const auto& slot = m_Slots[index];
            if (!slot.inUse.load(std::memory_order_acquire)) return HandleType::Invalid();

            auto generation = slot.generation.load(std::memory_order_relaxed);
            return HandleType(static_cast<uint32_t>(index), generation);
        }

        /// @brief Get the generation of a specific slot
        /// @param index Slot index to query
        /// @return Current generation of this slot
        ///
        /// Useful for advanced handle management and debugging scenarios.
        /// The generation counter is incremented each time a slot is allocated.
        uint32_t GetSlotGeneration(size_t index) const
        {
            if (index >= CAPACITY) return 0;
            return m_Slots[index].generation.load(std::memory_order_relaxed);
        }

    private:
        /// @brief Internal slot structure for object storage
        ///
        /// Each slot contains:
        /// - Raw storage for one object of type T
        /// - Atomic flag indicating if a slot is in use
        /// - Atomic generation counter for handle validation
        struct Slot
        {
            /// @brief Raw storage for an object (properly aligned)
            alignas(T) std::byte storage[sizeof(T)];

            /// @brief Atomic flag indicating if a slot contains a valid object
            std::atomic<bool> inUse{false};

            /// @brief Generation counter for ABA prevention
            std::atomic<uint32_t> generation{0};

            /// @brief Get typed pointer to storage
            /// @return Pointer to a stored object (maybe uninitialized)
            T* GetObject() noexcept
            {
                return reinterpret_cast<T*>(storage);
            }

            /// @brief Get const typed pointer to storage
            /// @return Const pointer to a stored object (maybe uninitialized)
            const T* GetObject() const noexcept
            {
                return reinterpret_cast<const T*>(storage);
            }
        };

        // === Private Members ===

        /// @brief Array of storage slots
        std::array<Slot, CAPACITY> m_Slots;

        /// @brief Hint for the next slot to check during allocation (lock-free optimization)
        std::atomic<uint32_t> m_NextSlot{0};

        /// @brief Current number of allocated objects (for stats)
        std::atomic<size_t> m_UsedCount{0};

        // === Private Methods ===

        /// @brief Find and claim a free slot for allocation
        /// @return Index of claimed slot, or CAPACITY if no slots available
        ///
        /// Uses atomic compare-and-swap to claim slots in a lock-free manner.
        /// Implements linear search with wraparound starting from the m_NextSlot hint.
        size_t FindAndClaimFreeSlot();

        /// @brief Validate handle against the current slot state
        /// @param handle Handle to validate
        /// @param slotIndex Output parameter for slot index if valid
        /// @return true if the handle is valid and matches the current slot generation
        bool ValidateHandle(HandleType handle, size_t& slotIndex) const;
    };

    // ==============================================================
    // Template Implementation
    // ==============================================================

    template <typename T, size_t CAPACITY>
    FixedStorage<T, CAPACITY>::FixedStorage() = default;

    template <typename T, size_t CAPACITY>
    FixedStorage<T, CAPACITY>::~FixedStorage()
    {
        // Release all allocated objects to ensure proper destruction
        for (size_t i = 0; i < CAPACITY; ++i)
        {
            if (auto& slot = m_Slots[i]; slot.inUse.load(std::memory_order_acquire))
            {
                // Destruct object manually to avoid memory leaks
                slot.GetObject()->~T();
                slot.inUse.store(false, std::memory_order_release);
            }
        }
    }

    template <typename T, size_t CAPACITY>
    template <typename... Args>
    typename FixedStorage<T, CAPACITY>::HandleType FixedStorage<T, CAPACITY>::Allocate(
        Args&&... args)
    {
        // Find and claim a free slot
        size_t slotIndex = FindAndClaimFreeSlot();
        if (slotIndex >= CAPACITY)
        {
            VA_ENGINE_WARN(
                "[FixedStorage<{}>] The storage is full ({}/{} slots used).",
                typeid(T).name(),
                GetUsedSlots(),
                CAPACITY);

            return HandleType::Invalid();
        }

        auto& slot = m_Slots[slotIndex];

        // Increment generation for new allocation
        uint32_t generation = slot.generation.fetch_add(1, std::memory_order_acq_rel) + 1;

        try
        {
            // Construct an object in-place with perfect forwarding
            T* object = slot.GetObject();
            new(object) T(std::forward<Args>(args)...);

            // Update statistics
            m_UsedCount.fetch_add(1, std::memory_order_relaxed);

            // Update hint for next allocation
            m_NextSlot.store((slotIndex + 1) % CAPACITY, std::memory_order_relaxed);

            return HandleType(static_cast<uint32_t>(slotIndex), generation);
        }
        catch (...)
        {
            // Construction failed - release claimed slot
            slot.generation.fetch_sub(1, std::memory_order_acq_rel);
            slot.inUse.store(false, std::memory_order_release);

            VA_ENGINE_ERROR(
                "[FixedStorage<{}>] Failed to construct object in slot {}.",
                typeid(T).name(),
                slotIndex);

            return HandleType::Invalid();
        }
    }

    template <typename T, size_t CAPACITY>
    bool FixedStorage<T, CAPACITY>::Release(HandleType handle)
    {
        size_t slotIndex;
        if (!ValidateHandle(handle, slotIndex))
        {
            return false;
        }

        auto& slot = m_Slots[slotIndex];

        // Destruct object
        slot.GetObject()->~T();

        // Mark slot as free
        slot.inUse.store(false, std::memory_order_release);

        // Update stats
        m_UsedCount.fetch_sub(1, std::memory_order_relaxed);

        return true;
    }

    template <typename T, size_t CAPACITY>
    T* FixedStorage<T, CAPACITY>::Get(HandleType handle)
    {
        size_t slotIndex;
        if (!ValidateHandle(handle, slotIndex))
        {
            return nullptr;
        }

        return m_Slots[slotIndex].GetObject();
    }

    template <typename T, size_t CAPACITY>
    const T* FixedStorage<T, CAPACITY>::Get(HandleType handle) const
    {
        size_t slotIndex;
        if (!ValidateHandle(handle, slotIndex))
        {
            return nullptr;
        }

        return m_Slots[slotIndex].GetObject();
    }

    template <typename T, size_t CAPACITY>
    bool FixedStorage<T, CAPACITY>::IsValid(HandleType handle) const
    {
        size_t slotIndex;

        return ValidateHandle(handle, slotIndex);
    }

    template <typename T, size_t CAPACITY>
    size_t FixedStorage<T, CAPACITY>::FindAndClaimFreeSlot()
    {
        const uint32_t startSlot = m_NextSlot.load(std::memory_order_relaxed);

        // Search for a free slot with wraparound
        for (size_t attempts = 0; attempts < CAPACITY; ++attempts)
        {
            size_t slotIndex = (startSlot + attempts) % CAPACITY;
            auto& slot = m_Slots[slotIndex];

            // Try to claim this slot
            if (auto expected = false; slot.inUse.compare_exchange_weak(
                expected,
                true,
                std::memory_order_acquire,
                std::memory_order_relaxed))
            {
                return slotIndex;
            }
        }

        return CAPACITY; // No free slots found - return an invalid handle
    }

    template <typename T, size_t CAPACITY>
    bool FixedStorage<T, CAPACITY>::ValidateHandle(HandleType handle, size_t& slotIndex) const
    {
        if (!handle.IsValid()) return false;

        slotIndex = handle.GetIndex();
        if (slotIndex >= CAPACITY) return false;

        const auto& slot = m_Slots[slotIndex];

        if (!slot.inUse.load(std::memory_order_acquire)) return false;

        // Check generation to prevent ABA problems
        uint32_t curGen = slot.generation.load(std::memory_order_relaxed);
        if (curGen != handle.GetGeneration()) return false;

        return true;
    }
}
