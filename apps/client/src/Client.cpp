//
// Created by Michael Desmedt on 12/05/2025.
//
#include <VoidArchitect/Engine/Common/VoidArchitect.hpp>
#include <VoidArchitect/Engine/Client/ClientApplication.hpp>

#include "TestLayer.hpp"

class Client final : public VoidArchitect::ClientApplication
{
public:
    Client() = default;

protected:
    void InitializeSubsystems() override
    {
        ClientApplication::InitializeSubsystems();
        PushLayer(new TestLayer());
    }
};

VoidArchitect::Application* VoidArchitect::CreateApplication() { return new Client(); }
