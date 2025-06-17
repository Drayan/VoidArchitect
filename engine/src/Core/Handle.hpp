//
// Created by Michael Desmedt on 17/06/2025.
//
#pragma once

namespace VoidArchitect
{
    /// @brief Generic handle template with generation counter for ABA prevention
    ///
    /// This template provides a robust handle system that can be used throughout
    /// the engine for safe resource referencing. The generation counter prevents
    /// ABA problems where a handle might accidentally reference a different object
    /// that was allocated in the same slot after the original was freed. It can
    /// also be used to detect if a resource changed.
    ///
    /// @tparam T Tag type for type safety (usually a forward-declared class)
    /// @tparam GenerationBits Number of bits to use for generation counter (default: 8)
    ///
    /// Usage examples:
    /// @code
    /// // Define type handles
    /// using TextureHandle = Handle<class TextureTag>;
    /// using MaterialHandle = Handle<class MaterialTag>;
    /// using JobHandle = Handle<class JobTag>;
    ///
    /// // Create and validate handles
    /// auto handle = TextureHandle{42, 5}; // index = 42, generation = 5
    /// if (handle.IsValid())
    /// {
    ///     // Use a handle safely
    /// }
    ///
    /// // Invalid handle
    /// auto invalid = TextureHandle::Invalid();
    /// assert(!invalid.IsValid());
    /// @endcode
    template <typename T, uint8_t GenerationBits = 8>
    struct Handle
    {
        /// @brief Number of bits available for index
        static constexpr uint32_t INDEX_BITS = 32 - GenerationBits;

        /// @brief Maximum valid index value
        static constexpr uint32_t MAX_INDEX = (1u << INDEX_BITS) - 1;

        /// @brief Reserved index value for invalid handles
        static constexpr uint32_t INVALID_INDEX = MAX_INDEX;

        /// @brief Maximum generation value before wrapping
        static constexpr uint32_t MAX_GENERATION = (1u << GenerationBits) - 1;

        // === Data Members ===

        /// @brief Index into the storage array (24 bits by default)
        uint32_t index : INDEX_BITS;

        /// @brief Generation counter for ABA prevention and change detection (8 bits by default)
        uint32_t generation : GenerationBits;

        // === Constructors ===

        /// @brief Default constructor creates an invalid handle
        constexpr Handle() noexcept
            : index(INVALID_INDEX),
              generation(0)
        {
        }

        /// @brief Construct handle with a specific index and generation
        /// @param idx Index value (will be clamped to MAX_INDEX)
        /// @param gen Generation value (will be wrapped if > MAX_GENERATION)
        constexpr Handle(const uint32_t idx, const uint32_t gen) noexcept
            : index(idx <= MAX_INDEX ? idx : INVALID_INDEX),
              generation(gen & MAX_GENERATION)
        {
        }

        // === Validation ===

        /// @brief Check if the handle is valid
        /// @return true if the handle has a valid index, false otherwise
        constexpr bool IsValid() const noexcept
        {
            return index != INVALID_INDEX;
        }

        /// @brief Get the handle index value
        /// @return Index portion of the handle
        constexpr uint32_t GetIndex() const noexcept
        {
            return index;
        }

        /// @brief Get the handle generation value
        /// @return Generation portion of the handle
        constexpr uint32_t GetGeneration() const noexcept
        {
            return generation;
        }

        // === Static Factory Methods ===

        /// @brief Create an explicitly invalid handle
        /// @return Handle that will return false for IsValid()
        static constexpr Handle Invalid() noexcept
        {
            return Handle{INVALID_INDEX, 0};
        }

        /// @brief Create a handle with next generation for given index
        /// @param idx Index value
        /// @param currentGen Current generation value
        /// @return New handle with incremented generation
        static constexpr Handle NextGeneration(
            const uint32_t idx,
            const uint32_t currentGen) noexcept
        {
            return Handle{idx, (currentGen + 1) & MAX_GENERATION};
        }

        // === Comparison Operators ===

        /// @brief Equality comparison
        /// @param other Handle to compare with
        /// @return true if both index AND generation match
        constexpr bool operator==(const Handle& other) const noexcept
        {
            return index == other.index && generation == other.generation;
        }

        /// @brief Inequality comparison
        /// @param other Handle to compare with
        /// @return true if either index OR generation differ
        constexpr bool operator!=(const Handle& other) const noexcept
        {
            return !(*this == other);
        }

        /// @brief Less-than comparison
        /// @param other Handle to compare with
        /// @return true if this handle is "less than" other
        ///
        /// @note Comparison is primarily by index, then by generation
        constexpr bool operator<(const Handle& other) const noexcept
        {
            if (index != other.index) return index < other.index;

            return generation < other.generation;
        }

        // === Hash support ===

        /// @brief Get hash value for use in hash tables
        /// @return Combined hash of index and generation
        constexpr uint32_t GetHash() const noexcept
        {
            return (static_cast<uint32_t>(index) << GenerationBits) | generation;
        }

        // === Debug support ===

        /// @brief Get packed representation as single uint32_t
        /// @return Handle data packed into 32-bit integer
        ///
        /// @note Useful for debugging and serialization
        constexpr uint32_t GetPacked() const noexcept
        {
            return GetHash();
        }

        /// @brief Create a handle from packed representation
        /// @param packed handle data from GetPacked()
        /// @return Reconstructed handle
        static constexpr Handle FromPacked(const uint32_t packed) noexcept
        {
            uint32_t idx = packed >> GenerationBits;
            uint32_t gen = packed & MAX_GENERATION;
            return Handle{idx, gen};
        }
    };

    /// @brief Hash functor for Handle types
    /// @tparam T handle tag type
    /// @tparam GenerationBits Number of generation bits
    ///
    /// Enables use of Handle types in std::unordered_map and similar containers:
    /// @code
    /// std::unordered_map<TextureHandle, TextureData, HandleHash<class TextureTag>> textureMap;
    /// @endcode
    template <typename T, uint8_t GenerationBits = 8>
    struct HandleHash
    {
        /// @brief Compute hash value for a handle
        /// @param handle Handle to hash
        /// @return Hash value suitable for hash table use
        constexpr std::size_t operator()(const Handle<T, GenerationBits>& handle) const noexcept
        {
            return static_cast<std::size_t>(handle.GetHash());
        }
    };
}
