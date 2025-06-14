//
// Created by Michael Desmedt on 13/06/2025.
//
#include "Transform.hpp"

namespace VoidArchitect
{
    void Transform::SetLocalPosition(const Math::Vec3& position)
    {
        m_LocalPosition = position;
        MarkDirty();
    }

    void Transform::SetLocalPosition(const float x, const float y, const float z)
    {
        m_LocalPosition = Math::Vec3(x, y, z);
        MarkDirty();
    }

    void Transform::SetLocalRotation(const Math::Quat& rotation)
    {
        m_LocalRotation = rotation;
        MarkDirty();
    }

    void Transform::SetLocalRotation(const float pitch, const float yaw, const float roll)
    {
        m_LocalRotation = Math::Quat::FromEuler(pitch, yaw, roll);
        MarkDirty();
    }

    void Transform::SetLocalScale(const Math::Vec3& scale)
    {
        m_LocalScale = scale;
        MarkDirty();
    }

    void Transform::SetLocalScale(const float x, const float y, const float z)
    {
        m_LocalScale = Math::Vec3(x, y, z);
        MarkDirty();
    }

    void Transform::SetParent(Transform* parent)
    {
        if (parent == m_Parent) return;

        if (m_Parent)
        {
            auto it = std::ranges::find(m_Parent->m_Children, this);
            if (it != m_Parent->m_Children.end())
            {
                m_Parent->m_Children.erase(it);
            }
        }

        m_Parent = parent;
        if (m_Parent)
        {
            m_Parent->m_Children.push_back(this);
        }

        MarkDirty();
    }

    void Transform::SetParentKeepWorldTransform(Transform* parent)
    {
        auto worldPos = GetWorldPosition();
        auto worldRot = GetWorldRotation();
        auto worldScale = GetWorldScale();

        SetParent(parent);

        if (m_Parent)
        {
            const auto parentWorldInverse = Math::Mat4::Inverse(m_Parent->GetWorldTransform());
            const auto desiredWorldTransform = Math::Mat4::FromTRS(worldPos, worldRot, worldScale);
            const auto newLocalTransform = parentWorldInverse * desiredWorldTransform;

            newLocalTransform.ToTRS(m_LocalPosition, m_LocalRotation, m_LocalScale);
        }
        else
        {
            m_LocalPosition = worldPos;
            m_LocalRotation = worldRot;
            m_LocalScale = worldScale;
        }

        MarkDirty();
    }

    const Math::Mat4& Transform::GetLocalTransform() const
    {
        if (m_LocalDirty)
        {
            m_LocalTransform = Math::Mat4::FromTRS(m_LocalPosition, m_LocalRotation, m_LocalScale);
            m_LocalDirty = false;
        }

        return m_LocalTransform;
    }

    const Math::Vec3 Transform::GetWorldPosition() const
    {
        return GetWorldTransform().GetTranslation();
    }

    const Math::Quat Transform::GetWorldRotation() const
    {
        return GetWorldTransform().GetRotation();
    }

    const Math::Vec3 Transform::GetWorldScale() const
    {
        return GetWorldTransform().GetScale();
    }

    const Math::Mat4& Transform::GetWorldTransform() const
    {
        if (m_WorldDirty)
        {
            if (m_Parent)
            {
                m_WorldTransform = m_Parent->GetWorldTransform() * GetLocalTransform();
            }
            else
            {
                m_WorldTransform = GetLocalTransform();
            }

            m_WorldDirty = false;
        }

        return m_WorldTransform;
    }

    void Transform::MarkDirty() const
    {
        m_LocalDirty = true;
        MarkWorldDirtyRecursive();
    }

    void Transform::MarkWorldDirtyRecursive() const
    {
        m_WorldDirty = true;

        for (const auto* child : m_Children) child->MarkWorldDirtyRecursive();
    }
}
