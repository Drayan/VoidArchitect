//
// Created by Michael Desmedt on 21/05/2025.
//
#include "TestLayer.hpp"

#include "Core/Logger.hpp"
#include "Systems/Renderer/Camera.hpp"
#include "Systems/Renderer/RenderCommand.hpp"

void TestLayer::OnAttach()
{
    VA_APP_TRACE("[TestLayer] OnAttach.");
    auto& camera = VoidArchitect::Renderer::RenderCommand::GetMainCamera();
    m_DebugCameraController =
        std::make_unique<VoidArchitect::Renderer::DebugCameraController>(camera);

    camera.SetPosition({0.0f, 0.0f, 3.0f});
}

void TestLayer::OnDetach()
{
    VA_APP_TRACE("[TestLayer] OnDetach.");
}

void TestLayer::OnUpdate(float deltaTime)
{
    m_DebugCameraController->OnUpdate(deltaTime);
}

void TestLayer::OnEvent(VoidArchitect::Event& e)
{
    m_DebugCameraController->OnEvent(e);
}


