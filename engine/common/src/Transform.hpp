//
// Created by Michael Desmedt on 13/06/2025.
//
#pragma once
#include "Math/Mat4.hpp"
#include "Math/Quat.hpp"
#include "Math/Vec3.hpp"

namespace VoidArchitect
{
    class Transform
    {
    public:
        void SetLocalPosition(const Math::Vec3& position);
        void SetLocalPosition(float x, float y, float z);
        void SetLocalRotation(const Math::Quat& rotation);
        void SetLocalRotation(float pitch, float yaw, float roll);
        void SetLocalScale(const Math::Vec3& scale);
        void SetLocalScale(float x, float y, float z);
        void SetParent(Transform* parent);
        void SetParentKeepWorldTransform(Transform* parent);

        Transform* GetParent() const { return m_Parent; }
        const VAArray<Transform*>& GetChildren() const { return m_Children; }
        const Math::Vec3& GetLocalPosition() const { return m_LocalPosition; }
        const Math::Quat& GetLocalRotation() const { return m_LocalRotation; }
        const Math::Vec3& GetLocalScale() const { return m_LocalScale; }
        const Math::Mat4& GetLocalTransform() const;
        const Math::Vec3 GetWorldPosition() const;
        const Math::Quat GetWorldRotation() const;
        const Math::Vec3 GetWorldScale() const;
        const Math::Mat4& GetWorldTransform() const;

    private:
        void MarkDirty() const;
        void MarkWorldDirtyRecursive() const;

        Math::Vec3 m_LocalPosition = Math::Vec3::Zero();
        Math::Quat m_LocalRotation = Math::Quat::Identity();
        Math::Vec3 m_LocalScale = Math::Vec3::One();

        Transform* m_Parent = nullptr;
        VAArray<Transform*> m_Children;

        mutable Math::Mat4 m_LocalTransform;
        mutable Math::Mat4 m_WorldTransform;
        mutable bool m_LocalDirty = true;
        mutable bool m_WorldDirty = true;
    };
}
