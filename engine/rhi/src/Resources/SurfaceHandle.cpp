//
// Created by Michael Desmedt on 13/07/2025.
//
#include "SurfaceHandle.hpp"

namespace VoidArchitect::RHI
{
    SurfaceHandle::SurfaceHandle(
        void* data,
        const Platform::RHI_API_TYPE apiType,
        const CreationType type,
        void* additionalData)
        : m_ApiType(apiType)
    {
        switch (type)
        {
            case CreationType::Deferred:
            {
                m_CreationData = data;
                m_NativeHandle = nullptr;
                m_State = State::Deferred;
                break;
            }

            case CreationType::Finalized:
            {
                m_NativeHandle = data;
                m_CreationData = additionalData;
                m_State = (data != nullptr) ? State::Finalized : State::Invalid;
                break;
            }
        }
    }

    SurfaceHandle SurfaceHandle::CreateDeferred(
        void* creationData,
        const Platform::RHI_API_TYPE apiType)
    {
        return SurfaceHandle(creationData, apiType, CreationType::Deferred);
    }

    SurfaceHandle SurfaceHandle::CreateFinalized(
        void* nativeHandle,
        const Platform::RHI_API_TYPE apiType,
        void* creationData)
    {
        return SurfaceHandle(nativeHandle, apiType, CreationType::Finalized, creationData);
    }

    SurfaceHandle::SurfaceHandle(SurfaceHandle&& other) noexcept
        : m_NativeHandle(other.m_NativeHandle),
          m_State(other.m_State),
          m_CreationData(other.m_CreationData),
          m_ApiType(other.m_ApiType)
    {
        other.Reset();
    }

    SurfaceHandle& SurfaceHandle::operator=(SurfaceHandle&& other) noexcept
    {
        if (this != &other)
        {
            m_NativeHandle = other.m_NativeHandle;
            m_CreationData = other.m_CreationData;
            m_ApiType = other.m_ApiType;
            m_State = other.m_State;

            other.Reset();
        }
        return *this;
    }

    void SurfaceHandle::Finalize(void* nativeHandle)
    {
        if (m_State != State::Deferred) return;

        m_NativeHandle = nativeHandle;
        m_State = (nativeHandle != nullptr) ? State::Finalized : State::Invalid;
    }

    void SurfaceHandle::Reset()
    {
        m_NativeHandle = nullptr;
        m_State = State::Invalid;
        m_CreationData = nullptr;
        m_ApiType = Platform::RHI_API_TYPE::None;
    }
}
