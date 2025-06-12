//
// Created by Michael Desmedt on 31/05/2025.
//
#pragma once
#include "Core/Math/Vec2.hpp"
#include "Core/Math/Vec3.hpp"
#include "Core/Uuid.hpp"
#include "Core/Math/Vec4.hpp"

namespace VoidArchitect
{
    class IBuffer;
}

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
            Math::Vec3 Normal;
            Math::Vec2 UV0;
            Math::Vec4 Tangent;
        };

        using MeshHandle = uint32_t;
        static constexpr MeshHandle InvalidMeshHandle = std::numeric_limits<uint32_t>::max();

        class IMesh
        {
            friend class VoidArchitect::MeshSystem;

        public:
            virtual ~IMesh() = default;

            virtual IBuffer* GetVertexBuffer() const = 0;
            virtual IBuffer* GetIndexBuffer() const = 0;
            virtual uint32_t GetIndicesCount() const = 0;

        protected:
            explicit IMesh(std::string name);
            virtual void Release() = 0;

            std::string m_Name;
        };
    } // namespace Resources
} // namespace VoidArchitect
