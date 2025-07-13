//
// Created by Michael Desmedt on 12/05/2025.
//
#pragma once

extern VoidArchitect::Application* VoidArchitect::CreateApplication();

// ReSharper disable once CppNonInlineFunctionDefinitionInHeaderFile
// NOTE : The main function cannot be defined as inline.
int main()
{
    VoidArchitect::Logger::Initialize();
    VA_ENGINE_INFO("Logging system initialized.");

    VA_ENGINE_INFO("Starting application...");
    const auto app = VoidArchitect::CreateApplication();
    app->Initialize();
    app->Run();
    VA_ENGINE_INFO("Application ended. Shutting down...");
    delete app;

    VA_ENGINE_INFO("Shutting down logging system...");
    VoidArchitect::Logger::Shutdown();
}
