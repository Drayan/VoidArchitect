//
// Created by Michael Desmedt on 17/06/2025.
//
#pragma once

namespace VoidArchitect::Testing
{
    /// @brief Test execution result with timing and status information
    struct TestResult
    {
        bool passed{false}; ///< Whether test passed or failed
        std::string testName; ///< Name of the test
        std::chrono::nanoseconds executionTime; ///< Time taken to execute the test
        std::string errorMessage; ///< Error message if test failed

        /// @brief Create successful test result
        /// @param name Test name
        /// @param duration Execution time
        /// @return TestResult indicating success
        static TestResult Success(const std::string& name, const std::chrono::nanoseconds& duration)
        {
            return TestResult{true, name, duration, ""};
        }

        /// @brief Create failed test result
        /// @param name Test name
        /// @param duration Execution time
        /// @param error Error message
        /// @return TestResult indicating failure
        static TestResult Failure(
            const std::string& name,
            const std::chrono::nanoseconds& duration,
            const std::string& error)
        {
            return TestResult{false, name, duration, error};
        }
    };

    /// @brief Summary of test run results
    struct TestRunSummary
    {
        size_t totalTests{0}; ///< Total number of tests executed
        size_t passedTests{0}; ///< Number of tests passed
        size_t failedTests{0}; ///< Number of tests failed
        std::chrono::nanoseconds totalExecutionTime{0}; ///< Total time for all tests
        std::vector<TestResult> results; ///< Individual test results

        /// @brief Check if all tests passed
        /// @return true if no tests failed
        bool AllTestsPassed() const { return failedTests == 0; }

        /// @brief Get success percentage
        /// @return Percentage of tests that passed (0.0 to 1.0)
        float GetSuccessRate() const
        {
            return totalTests > 0
                ? static_cast<float>(passedTests) / static_cast<float>(totalTests)
                : 0.0f;
        }
    };

    /// @brief Test function signature
    ///
    /// Test functions should return true for success, false for failure.
    /// They should handle their own error reporting to stdout/stderr.
    using TestFunction = std::function<bool()>;

    /// @brief Simple test runner for VoidArchitect engine components
    ///
    /// TestRunner provides a lightweight framework for registering and executing
    /// unit tests throughout the engine. Tests are automatically discovered and
    /// can be run individually or as a complete suite.
    ///
    /// Key features:
    /// - Automatic test registration using macros
    /// - Individual or batch test execution
    /// - Timing and performance measurement
    /// - CMake integration ready
    /// - CI/CD friendly output formatting
    ///
    /// Usage example:
    /// @code
    /// // Register a test (usually at file scope)
    /// VA_REGISTER_TEST(MyComponentBasics, []()
    /// {
    ///     MyComponent comp;
    ///     return comp.IsValid() && comp.DoSomething();
    /// });
    ///
    /// // Run all tests
    /// int main()
    /// {
    ///     return TestRunner::RunAllTests();
    /// }
    /// @endcode
    class TestRunner
    {
    public:
        /// @brief Register a test function with the runner
        /// @param testName Unique name for the test (should be descriptive)
        /// @param testFunc Function to execute for this test
        ///
        /// This method is typically called via the VA_REGISTER_TEST macro
        /// rather that directly.
        static void RegisterTest(const char* testName, TestFunction testFunc);

        /// @brief Execute all registered tests
        /// @param verboseOutput If true, print detailled output for each test
        /// @return 0 if all tests passed, 1 if any tests failed
        ///
        /// Executes all tests that have been registered with RegisterTest().
        /// Results are printed to stdout in a format suitable for CI/CD systems.
        static int RunAllTests(bool verboseOutput = true);

        /// @brief Execute a specific test by name
        /// @param testName Name of test to execute
        /// @param verboseOutput If true, print detailed output
        /// @return 0 if test passed, 1 is test failed or not found
        static int RunSpecificTest(const char* testName, bool verboseOutput = true);

        /// @brief Execute tests matching a pattern
        /// @param pattern Substring that test names must contain
        /// @param verboseOutput If true, print detailed output
        /// @return 0 if all matching tests passed, 1 if any failed
        static int RunTestsMatching(const char* pattern, bool verboseOutput = true);

        /// @brief Get list of all registered test names
        /// @return Vector of test names
        static std::vector<std::string> GetRegisteredTestNames();

        /// @brief Check if a test with given name is registered
        /// @param testName Name to check
        /// @return true if test exists
        static bool IsTestRegistered(const char* testName);

        /// @brief Get number of registered tests
        /// @return Total number of tests available
        static size_t GetTestCount();

        /// @brief Execute tests and return detailed summary
        /// @param testNames Vector of test names to execute (empty = all tests)
        /// @param verboseOutput If true, print progress during execution
        /// @return TestRunSummary with detailed results
        static TestRunSummary ExecuteTests(
            const std::vector<std::string>& testNames = {},
            bool verboseOutput = true);

    private:
        /// @brief Internal test registry entry
        struct TestEntry
        {
            std::string name; ///< Test name
            TestFunction func; ///< Test function to execute
        };

        /// @brief Get reference to test registry (singleton pattern)
        /// @return Reference to static test registry
        static std::vector<TestEntry>& GetTestRegistry();

        /// @brief Execute a single test with timing
        /// @param entry Test entry to execute
        /// @param verboseOutput Whether to print progress
        /// @return TestResult with execution details
        static TestResult ExecuteSingleTest(const TestEntry& entry, bool verboseOutput);

        /// @brief Print test run summary in a readable format
        /// @param summary Test run results to display
        static void PrintSummary(const TestRunSummary& summary);

        /// @brief Print header for test execution
        /// @param testCount Number of tests being executed
        static void PrintHeader(size_t testCount);
    };
}

/// @brief Macro to register a test function with the test runner
/// @param name Unique test name (will be stringified)
/// @param func Function or lambda that returns bool (true = pass, false = fail)
///
/// This macro should be used at file scope to register tests. The test
/// will be automatically discovered and included in test runs.
///
/// Example usage:
/// @code
/// VA_REGISTER_TEST(CollectionsBasics, []()
/// {
///     VAArray<int> arr;
///     arr.push_back(42);
///     return arr.size() == 1 && arr[0] == 42;
/// });
/// @endcode
#define VA_REGISTER_TEST(name, func) \
    static bool s_##name##_test_registered = \
        (::VoidArchitect::Testing::TestRunner::RegisterTest(#name, func), true)

/// @brief Macro to register a test with custom test name string
/// @param nameStr String literal for test name
/// @param func Function or lambda that returns bool
#define VA_REGISTER_TEST_NAMED(nameStr, func) \
    static bool s_test_registered_##__COUNTER__ = \
        (::VoidArchitect::Testing::TestRunner::RegisterTest(nameStr, func), true)
