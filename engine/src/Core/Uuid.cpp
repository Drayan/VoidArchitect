//
// Created by Michael Desmedt on 22/05/2025.
//
#include "Uuid.hpp"

#include <random>

#include "Logger.hpp"

namespace VoidArchitect
{
    static std::random_device s_RandomDevice;
    static std::mt19937_64 s_RandomEngine(s_RandomDevice());
    static std::uniform_int_distribution<uint64_t> s_RandomDistribution;

    UUID::UUID()
        : m_UUID(s_RandomDistribution(s_RandomEngine))
    {
        if (m_UUID == InvalidUUID)
        {
            m_UUID = s_RandomDistribution(s_RandomEngine);
            VA_ENGINE_WARN("[UUID] Collision with InvalidUUID, regenerate.");
        }
    }

    UUID::UUID(const uint64_t uuid)
        : m_UUID(uuid)
    {
    }

    bool UUID::operator==(const UUID& other) const
    {
        return m_UUID == other.m_UUID;
    }

    bool UUID::operator==(const uint64_t uuid) const
    {
        return m_UUID == uuid;
    }
}
