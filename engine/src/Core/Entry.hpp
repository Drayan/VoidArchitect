//
// Created by Michael Desmedt on 12/05/2025.
//
#pragma once

extern VoidArchitect::Application* VoidArchitect::CreateApplication();

int main()
{
    // TODO Temp testing
    VoidArchitect::Logger::Initialize();
    VA_ENGINE_INFO("Logging system initialized.");

    VA_ENGINE_INFO("Starting application...");
    const auto app = VoidArchitect::CreateApplication();
    app->Run();
    delete app;

    VoidArchitect::Logger::Shutdown();
}
