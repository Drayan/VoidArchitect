//
// Created by Michael Desmedt on 17/06/2025.
//
#include <VoidArchitect.hpp>

#include "TestLayer.hpp"

/// @brief Test application that runs the test suite using a Layer
class TestApplication : public VoidArchitect::Application
{
public:
    /// @brief Constructor - Set up test layer
    ///
    /// Following VoidArchitect's application pattern exactly like the client,
    /// we push a TestLayer that will handle test execution.
    TestApplication()
    {
        VA_APP_INFO("VoidArchitect Test Application initializing...");

        // Push the test layer
        PushLayer(new VoidArchitect::Testing::TestLayer());

        VA_APP_INFO("Test layer added to application.");
    }
};

/// @brief Create the  test application
VoidArchitect::Application* VoidArchitect::CreateApplication() { return new TestApplication(); }
