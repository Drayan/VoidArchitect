//
// Created by Michael Desmedt on 21/05/2025.
//
#pragma once
#include <VoidArchitect/Engine/Client/Systems/Renderer/DebugCameraController.hpp>

class TestLayer
{
public:
    void OnAttach();
    void OnDetach();
    void OnFixedUpdate(float fixedTimestep);

private:
    std::unique_ptr<VoidArchitect::Renderer::DebugCameraController> m_DebugCameraController;
};
