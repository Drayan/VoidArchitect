//
// Created by Michael Desmedt on 21/05/2025.
//
#include "TestLayer.hpp"

#include <VoidArchitect/Engine/Common/Logger.hpp>
#include <VoidArchitect/Engine/Client/Systems/Renderer/Camera.hpp>
#include <VoidArchitect/Engine/Client/Systems/Renderer/RenderSystem.hpp>

#include <SDL3/SDL_keycode.h>

void TestLayer::OnAttach()
{
    VA_APP_TRACE("[TestLayer] OnAttach.");
    auto& camera = VoidArchitect::Renderer::g_RenderSystem->GetMainCamera();
    m_DebugCameraController = std::make_unique<VoidArchitect::Renderer::DebugCameraController>(
        camera);

    camera.SetPosition({0.f, 0.f, 3.f});
}

void TestLayer::OnDetach() { VA_APP_TRACE("[TestLayer] OnDetach."); }

void TestLayer::OnFixedUpdate(const float fixedTimestep)
{
    m_DebugCameraController->OnFixedUpdate(fixedTimestep);
}
