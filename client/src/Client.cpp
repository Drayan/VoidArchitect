//
// Created by Michael Desmedt on 12/05/2025.
//
#include <VoidArchitect.hpp>

class Client : public VoidArchitect::Application
{
public:
    Client() = default;
};

VoidArchitect::Application* VoidArchitect::CreateApplication() { return new Client(); }
