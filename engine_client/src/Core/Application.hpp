//
// Created by Michael Desmedt on 12/05/2025.
//
#pragma once

#include "Core.hpp"

namespace VoidArchitect
{

    class VA_API Application
    {
    public:
        Application();
        virtual ~Application();

        void Run();
    };

    Application* CreateApplication();
} // namespace VoidArchitect
