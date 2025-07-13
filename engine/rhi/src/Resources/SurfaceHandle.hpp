//
// Created by Michael Desmedt on 13/07/2025.
//
#pragma once

#include "../Interface/IRenderingHardware.hpp"

namespace VoidArchitect::RHI
{
    /// @brief Surface creation type discriminator
    enum class CreationType
    {
        Deferred, ///< Surface needs finalization with API context
        Finalized ///< Surface is ready to use immediately
    };

    /// @brief Platform-agnostic wrapper for graphics API surface handles
    ///
    /// Encapsulates native surface handles (VkSurfaceKHR, HWND, etc.)
    /// with type-safe access and lifetime management. Supports both immediate
    /// surfaces and deferred creation for APIs requiring additional context.
    class SurfaceHandle
    {
    public:
        /// @brief Surface creation state
        enum class State
        {
            Invalid, ///< No surface data
            Deferred, ///< Creation info stored, need finalization
            Finalized ///< Native surface created and ready
        };

        /// @brief Create invalid surface handle
        SurfaceHandle() = default;

        /// @brief Create surface handle with explicit type
        /// @param data Platform-specific data (creation context or native handle)
        /// @param apiType Graphics API this surface belongs to
        /// @param type Whether data is creation context or native handle
        /// @param additionalData Optional additional context (for finalized surfaces)
        SurfaceHandle(
            void* data,
            Platform::RHI_API_TYPE apiType,
            CreationType type,
            void* additionalData = nullptr);

        /// @brief Create deferred surface using factory method
        /// @param creationData Platform-specific creation context
        /// @param apiType Graphics API this surface belongs to
        /// @return Deferred surface handle
        static SurfaceHandle CreateDeferred(void* creationData, Platform::RHI_API_TYPE apiType);

        /// @brief Create finalized surface using factory method
        /// @param nativeHandle Native surface pointer
        /// @param apiType Graphics API this surface belongs to
        /// @param creationData Original creation context (optional)
        /// @return Finalized surface handle
        static SurfaceHandle CreateFinalized(
            void* nativeHandle,
            Platform::RHI_API_TYPE apiType,
            void* creationData = nullptr);

        // Allow moving
        SurfaceHandle(SurfaceHandle&& other) noexcept;
        SurfaceHandle& operator=(SurfaceHandle&& other) noexcept;

        // Don't allow copying
        SurfaceHandle(const SurfaceHandle&) = delete;
        SurfaceHandle& operator=(const SurfaceHandle&) = delete;

        /// @brief Destructor - does not destroy native handle
        ~SurfaceHandle() = default;

        /// @brief Finalize surface with native handle
        /// @param nativeHandle Created native surface
        ///
        /// Transition from Deferred to Finalized state. Called by RHI
        /// after completing two-phase surface creation.
        void Finalize(void* nativeHandle);

        /// @brief Invalidate handle without destroying native handle
        void Reset();

        /// @brief Get typed pointer to native handle
        /// @tparam T Expected surface type (e.g., VkSurfaceKHR)
        /// @return Typed pointer or nullptr if not finalized
        template <typename T>
        T GetAs() const
        {
            return m_State == State::Finalized ? static_cast<T>(m_NativeHandle) : nullptr;
        }

        /// @brief Get raw native handle
        /// @return Untyped pointer to native surface
        void* GetNativeHandle() const { return m_NativeHandle; }

        /// @brief Get creation context data
        /// @return Platform-specific creation data
        void* GetCreationData() const { return m_CreationData; }

        /// @brief Get graphics API type
        /// @return API this surface was created for
        Platform::RHI_API_TYPE GetApiType() const { return m_ApiType; }

        /// @brief Get current surface state
        /// @return Creation/finalization state
        State GetState() const { return m_State; }

        /// @brief Check if handle contains usable native surface
        /// @return true if finalized and ready to use
        bool IsReady() const { return m_State == State::Finalized && m_NativeHandle != nullptr; }

        /// @brief Check if handle has creation data for deferred creation
        /// @return true if has creation context
        bool IsDeferred() const { return m_State == State::Deferred && m_CreationData != nullptr; }

    private:
        /// @brief Native surface handle (VkSurfaceKHR, etc.) - only valid when finalized
        void* m_NativeHandle = nullptr;

        /// @brief Current surface state
        State m_State = State::Invalid;

        /// @brief Platform creation context (SDL_Window*, HWND, etc.)
        void* m_CreationData = nullptr;

        /// @brief Graphics API this surface belongs to
        Platform::RHI_API_TYPE m_ApiType = Platform::RHI_API_TYPE::None;
    };
}
