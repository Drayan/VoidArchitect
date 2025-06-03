//
// Created by Michael Desmedt on 21/05/2025.
//
#include "TestLayer.hpp"

#include "Core/Logger.hpp"
#include "Systems/Renderer/Camera.hpp"
#include "Systems/Renderer/RenderCommand.hpp"

#include <SDL3/SDL_keycode.h>

void TestLayer::OnAttach()
{
    VA_APP_TRACE("[TestLayer] OnAttach.");
    auto& camera = VoidArchitect::Renderer::RenderCommand::GetMainCamera();
    m_DebugCameraController =
        std::make_unique<VoidArchitect::Renderer::DebugCameraController>(camera);

    camera.SetPosition({0.f, 0.f, 3.f});
}

void TestLayer::OnDetach() { VA_APP_TRACE("[TestLayer] OnDetach."); }

void TestLayer::OnFixedUpdate(const float fixedTimestep)
{
    m_DebugCameraController->OnFixedUpdate(fixedTimestep);
}

void TestLayer::OnEvent(VoidArchitect::Event& e)
{
    m_DebugCameraController->OnEvent(e);

    auto dispatcher = VoidArchitect::EventDispatcher(e);
    dispatcher.Dispatch<VoidArchitect::KeyPressedEvent>(
        [](VoidArchitect::KeyPressedEvent& e)
        {
            if (e.GetKeyCode() == SDLK_T)
            {
                VoidArchitect::Renderer::RenderCommand::SwapTestTexture();
            }
            if (e.GetKeyCode() == SDLK_C)
            {
                VoidArchitect::Renderer::RenderCommand::SwapColor();
            }
            return false;
        });
}
