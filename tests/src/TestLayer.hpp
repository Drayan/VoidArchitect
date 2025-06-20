//
// Created by Michael Desmedt on 17/06/2025.
//
#pragma once

#include <Core/Layer.hpp>

namespace VoidArchitect::Testing
{
    /// @brief Layer that executes the test suite
    ///
    /// TestLayer integrates with VoidArchitect's layer system to run tests
    /// at the appropriated time during the application lifecycle. Tests are
    /// executed once during the first fixed update, then the application
    /// is signaled to shut down gracefully.
    class TestLayer : public VoidArchitect::Layer
    {
    public:
        TestLayer();
        virtual ~TestLayer() override;

        void OnFixedUpdate(const float fixedTimestep) override;
        void OnAttach() override;
        void OnDetach() override;

        /// @brief Check if tests have completed
        /// @return true if tests have been executed
        bool TestsCompleted() const { return m_TestsExecuted; }

        /// @brief Check if all tests passed
        /// @return true if tests passed, false if failed
        bool TestsPassed() const { return m_TestsPassed; }

    private:
        bool m_TestsExecuted{false}; ///< Whether tests have been run
        bool m_TestsPassed{false}; ///< Whether all tests passed

        /// @brief Execute the test suite
        void ExecuteTests();

        /// @brief Request application shutdown with the appropriate exit code
        void RequestShutdown();
    };
}
