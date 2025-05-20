//
// Created by Michael Desmedt on 21/05/2025.
//
#pragma once
#include "Core/Layer.hpp"
#include "Systems/Renderer/DebugCameraController.hpp"

class TestLayer : public VoidArchitect::Layer
{
public:
    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(float deltaTime) override;
    void OnEvent(VoidArchitect::Event& e) override;

private:
    std::unique_ptr<VoidArchitect::Renderer::DebugCameraController> m_DebugCameraController;
};
