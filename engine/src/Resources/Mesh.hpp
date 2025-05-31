//
// Created by Michael Desmedt on 31/05/2025.
//
#pragma once
#include "Core/Uuid.hpp"
#include "Core/Math/Vec2.hpp"
#include "Core/Math/Vec3.hpp"

namespace VoidArchitect
{
    class MeshSystem;

    namespace Platform
    {
        class IRenderingHardware;
    }

    namespace Resources
    {
        struct MeshVertex
        {
            Math::Vec3 Position;
            Math::Vec2 UV0;
        };

        class IMesh
        {
            friend class VoidArchitect::MeshSystem;

        public:
            virtual ~IMesh() = default;

            virtual void Bind(Platform::IRenderingHardware& rhi) = 0;

            virtual uint32_t GetIndicesCount() = 0;

        protected:
            explicit IMesh(std::string name);
            virtual void Release() = 0;

            std::string m_Name;
            uint32_t m_Handle;
            UUID m_UUID;
        };

        using MeshPtr = std::shared_ptr<IMesh>;
    } // Resources
} // VoidArchitect
