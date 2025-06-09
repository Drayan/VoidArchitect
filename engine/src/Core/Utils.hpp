//
// Created by Michael Desmedt on 08/06/2025.
//
#pragma once

namespace VoidArchitect
{
    template <class T>
    void HashCombine(std::size_t& seed, const T& v)
    {
        std::hash<T> hasher;
        // Inspired from boost::hash_combine
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
}

namespace std
{
    template <class Tuple, std::size_t Index = std::tuple_size<Tuple>::value - 1>
    struct TupleHasher
    {
        static void hash(std::size_t& seed, const Tuple& t)
        {
            TupleHasher<Tuple, Index - 1>::hash(seed, t);
            VoidArchitect::HashCombine(seed, std::get<Index>(t));
        }
    };

    // Cas de base pour la récursion (lorsqu'on atteint le premier élément)
    template <class Tuple>
    struct TupleHasher<Tuple, 0>
    {
        static void hash(std::size_t& seed, const Tuple& t)
        {
            VoidArchitect::HashCombine(seed, std::get<0>(t));
        }
    };

    // La spécialisation finale pour std::tuple elle-même
    template <typename... Ts>
    struct hash<tuple<Ts...>>
    {
        size_t operator()(const tuple<Ts...>& t) const
        {
            size_t seed = 0;
            // Si le tuple n'est pas vide, on commence le hachage
            if constexpr (sizeof...(Ts) > 0)
            {
                TupleHasher<tuple<Ts...>>::hash(seed, t);
            }
            return seed;
        }
    };
}
