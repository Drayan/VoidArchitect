//
// Created by Michael Desmedt on 12/05/2025.
//
#include <VoidArchitect.hpp>

#include "TestLayer.hpp"

class Client : public VoidArchitect::Application
{
public:
    Client()
    {
        PushLayer(new TestLayer());
    }
};

VoidArchitect::Application* VoidArchitect::CreateApplication() { return new Client(); }
