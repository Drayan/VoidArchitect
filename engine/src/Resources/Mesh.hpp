//
// Created by Michael Desmedt on 31/05/2025.
//
#pragma once
#include "Core/Math/Vec2.hpp"
#include "Core/Math/Vec3.hpp"

namespace VoidArchitect
{
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
        public:
            virtual ~IMesh() = default;

            virtual void Bind(Platform::IRenderingHardware& rhi) = 0;

            virtual uint32_t GetIndicesCount() = 0;
        };

        using MeshPtr = std::shared_ptr<IMesh>;
    } // Resources
} // VoidArchitect
