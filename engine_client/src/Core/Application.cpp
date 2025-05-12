//
// Created by Michael Desmedt on 12/05/2025.
//

#include "Application.hpp"

#include <iostream>

namespace VoidArchitect
{
    Application::Application() = default;
    Application::~Application() = default;

    void Application::Run()
    {
        std::cout << "Running application..." << std::endl;
        while (true)
            ;
    }
} // namespace VoidArchitect
