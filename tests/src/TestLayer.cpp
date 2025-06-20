//
// Created by Michael Desmedt on 17/06/2025.
//
#include "TestLayer.hpp"

#include "Core/TestRunner.hpp"

#include <Core/Logger.hpp>
#include <cstdlib>

namespace VoidArchitect::Testing
{
    TestLayer::TestLayer()
        : Layer("TestLayer")
    {
    }

    TestLayer::~TestLayer()
    {
    }

    void TestLayer::OnFixedUpdate(const float fixedTimestep)
    {
        // Execute tests only once during first update
        if (!m_TestsExecuted)
        {
            VA_APP_INFO("TestLayer::OnFixedUpdate - Executing tests...");
            ExecuteTests();
            m_TestsExecuted = true;

            RequestShutdown();
        }
    }

    void TestLayer::OnAttach()
    {
        VA_APP_INFO("TestLayer attached - test execution will begin on first update.");
    }

    void TestLayer::OnDetach()
    {
        Layer::OnDetach();
    }

    void TestLayer::ExecuteTests()
    {
        VA_APP_INFO("=== Starting VoidArchitect Test Suite ===");

        // Check if any test are registered
        auto testCount = TestRunner::GetTestCount();
        if (testCount == 0)
        {
            VA_APP_WARN("No tests registered. Make sure test files are properly linked.");
            m_TestsPassed = true; // No test = success
            return;
        }

        VA_APP_INFO("Found {} registered test(s).", testCount);

        // Execute all tests
        try
        {
            auto result = TestRunner::RunAllTests(true); // verbose = true
            m_TestsPassed = (result == 0);

            if (m_TestsPassed)
            {
                VA_APP_INFO("🎉 All tests PASSED!");
            }
            else
            {
                VA_APP_ERROR("❌ Some tests FAILED!");
            }
        }
        catch (const std::exception& e)
        {
            VA_APP_CRITICAL("FATAL ERROR during test execution: {}", e.what());
            m_TestsPassed = false;
        } catch (...)
        {
            VA_APP_CRITICAL("FATAL ERROR: Unknown exception during test execution.");
            m_TestsPassed = false;
        }

        VA_APP_INFO("=== Test Suite Execution Complete ===");
    }

    void TestLayer::RequestShutdown()
    {
        //TODO: Find a proper way to signal Application shutdown
        if (m_TestsPassed)
        {
            std::exit(0);
        }
        else
        {
            std::exit(1);
        }
    }
}
