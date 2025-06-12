//
// Created by Michael Desmedt on 31/05/2025.
//
#include "Mesh.hpp"

#include <utility>

namespace VoidArchitect::Resources
{
    IMesh::IMesh(std::string name)
        : m_Name(std::move(name))
    {
    }
} // namespace VoidArchitect::Resources
