//
// Created by Michael Desmedt on 22/05/2025.
//
#pragma once

namespace VoidArchitect
{
    class UUID
    {
    public:
        UUID();
        explicit UUID(uint64_t uuid);

        explicit operator uint64_t() const { return m_UUID; }
        bool operator==(const UUID& other) const;
        bool operator==(uint64_t uuid) const;

    private:
        uint64_t m_UUID;
    };

    inline UUID InvalidUUID(std::numeric_limits<uint64_t>::max());
} // namespace VoidArchitect

namespace std
{
    template <> struct hash<VoidArchitect::UUID>
    {
        std::size_t operator()(const VoidArchitect::UUID& uuid) const noexcept
        {
            return hash<uint64_t>()(static_cast<uint64_t>(uuid));
        }
    };
} // namespace std
