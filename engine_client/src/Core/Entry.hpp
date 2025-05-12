//
// Created by Michael Desmedt on 12/05/2025.
//

#pragma once
#include "Application.hpp"

extern VoidArchitect::Application* VoidArchitect::CreateApplication();
int main()
{
    const auto app = VoidArchitect::CreateApplication();
    app->Run();
    delete app;
}
