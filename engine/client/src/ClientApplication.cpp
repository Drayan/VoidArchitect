//
// Created by Michael Desmedt on 09/07/2025.
//
#include "../common/src/ClientApplication.hpp"

namespace VoidArchitect
{
#define BIND_EVENT_FN(x) [this](auto&& PH1) { return this->x(std::forward<decltype(PH1)>(PH1)); }
} // VoidArchitect
