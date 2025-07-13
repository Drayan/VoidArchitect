//
// Created by Michael Desmedt on 21/05/2025.
//
#pragma once
#include <VoidArchitect/Engine/Common/Layer.hpp>
#include <VoidArchitect/Engine/Client/Systems/Renderer/DebugCameraController.hpp>

class TestLayer : public VoidArchitect::Layer
{
public:
    void OnAttach() override;
    void OnDetach() override;
    void OnFixedUpdate(float fixedTimestep) override;
    void OnEvent(VoidArchitect::Event& e) override;

private:
    std::unique_ptr<VoidArchitect::Renderer::DebugCameraController> m_DebugCameraController;
};
