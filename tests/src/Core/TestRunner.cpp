//
// Created by Michael Desmedt on 17/06/2025.
//
#include "TestRunner.hpp"

namespace VoidArchitect::Testing
{
    void TestRunner::RegisterTest(const char* testName, TestFunction testFunc)
    {
        auto& registry = GetTestRegistry();

        // Check for duplicate test names
        for (const auto& entry : registry)
        {
            if (entry.name == testName)
            {
                std::cerr << "WARNING: Test '" << testName <<
                    "' is already registered. Skipping duplicate." << std::endl;
                return;
            }
        }

        registry.emplace_back(TestEntry{testName, std::move(testFunc)});
    }

    int TestRunner::RunAllTests(bool verboseOutput)
    {
        auto summary = ExecuteTests({}, verboseOutput);
        return summary.AllTestsPassed() ? 0 : 1;
    }

    int TestRunner::RunSpecificTest(const char* testName, bool verboseOutput)
    {
        auto summary = ExecuteTests({testName}, verboseOutput);
        if (summary.totalTests == 0)
        {
            std::cerr << "ERROR : Test '" << testName << "' not found." << std::endl;
            return 1;
        }

        return summary.AllTestsPassed() ? 0 : 1;
    }

    int TestRunner::RunTestsMatching(const char* pattern, bool verboseOutput)
    {
        std::vector<std::string> matchingTests;
        auto& registry = GetTestRegistry();

        for (const auto& entry : registry)
        {
            if (entry.name.find(pattern) != std::string::npos)
            {
                matchingTests.push_back(entry.name);
            }
        }

        if (matchingTests.empty())
        {
            std::cerr << "ERROR : No tests found matching pattern '" << pattern << "'." <<
                std::endl;
            return 1;
        }

        auto summary = ExecuteTests(matchingTests, verboseOutput);
        return summary.AllTestsPassed() ? 0 : 1;
    }

    std::vector<std::string> TestRunner::GetRegisteredTestNames()
    {
        std::vector<std::string> names;
        auto& registry = GetTestRegistry();

        names.reserve(registry.size());
        for (const auto& entry : registry)
        {
            names.push_back(entry.name);
        }

        std::sort(names.begin(), names.end());
        return names;
    }

    bool TestRunner::IsTestRegistered(const char* testName)
    {
        auto& registry = GetTestRegistry();
        return std::ranges::any_of(
            registry,
            [testName](const TestEntry& entry) { return entry.name == testName; });
    }

    size_t TestRunner::GetTestCount()
    {
        return GetTestRegistry().size();
    }

    TestRunSummary TestRunner::ExecuteTests(
        const std::vector<std::string>& testNames,
        bool verboseOutput)
    {
        auto& registry = GetTestRegistry();
        TestRunSummary summary;

        // Determine which tests to run
        std::vector<TestEntry> testsToRun;

        if (testNames.empty())
        {
            // Run all tests
            testsToRun = registry;
        }
        else
        {
            // Run specific tests
            for (const auto& testName : testNames)
            {
                auto it = std::ranges::find_if(
                    registry,
                    [&testName](const TestEntry& entry) { return entry.name == testName; });
                if (it != registry.end())
                {
                    testsToRun.push_back(*it);
                }
                else if (verboseOutput)
                {
                    std::cerr << "WARNING: Test '" << testName << "' not found." << std::endl;
                }
            }
        }

        if (testsToRun.empty())
        {
            if (verboseOutput)
            {
                std::cout << "No tests to execute." << std::endl;
            }
            return summary;
        }

        // sort tests by name for consistent execution order
        std::ranges::sort(
            testsToRun,
            [](const TestEntry& a, const TestEntry& b) { return a.name < b.name; });

        // Print header
        if (verboseOutput)
        {
            PrintHeader(testsToRun.size());
        }

        // Execute tests
        auto startTime = std::chrono::high_resolution_clock::now();

        for (const auto& entry : testsToRun)
        {
            TestResult result = ExecuteSingleTest(entry, verboseOutput);
            summary.results.push_back(result);
            summary.totalExecutionTime += result.executionTime;

            if (result.passed)
            {
                summary.passedTests++;
            }
            else
            {
                summary.failedTests++;
            }
        }

        summary.totalTests = testsToRun.size();

        // Print summary
        if (verboseOutput)
        {
            PrintSummary(summary);
        }

        return summary;
    }

    std::vector<TestRunner::TestEntry>& TestRunner::GetTestRegistry()
    {
        static std::vector<TestEntry> registry;
        return registry;
    }

    TestResult TestRunner::ExecuteSingleTest(const TestEntry& entry, bool verboseOutput)
    {
        if (verboseOutput)
        {
            std::cout << "Running " << std::setw(40) << std::left << entry.name << "...";
            std::cout.flush();
        }

        auto startTime = std::chrono::high_resolution_clock::now();
        bool testPassed = false;
        std::string errorMessage;

        try
        {
            testPassed = entry.func();
        }
        catch (const std::exception& e)
        {
            testPassed = false;
            errorMessage = std::string("Exception: ") + e.what();
        } catch (...)
        {
            testPassed = false;
            errorMessage = "Unknown exception";
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);

        if (verboseOutput)
        {
            if (testPassed)
            {
                std::cout << "✅ PASS";
            }
            else
            {
                std::cout << "❌ FAIL";
                if (!errorMessage.empty())
                {
                    std::cout << " (" << errorMessage << ")";
                }
            }

            // Print execution time
            auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration);
            std::cout << " [" << microseconds.count() << " μs]" << std::endl;
        }

        if (testPassed)
        {
            return TestResult::Success(entry.name, duration);
        }
        else
        {
            return TestResult::Failure(entry.name, duration, errorMessage);
        }
    }

    void TestRunner::PrintSummary(const TestRunSummary& summary)
    {
        std::cout << std::endl << std::string(60, '=') << std::endl;
        std::cout << "TEST SUMMARY" << std::endl;
        std::cout << std::string(60, '=') << std::endl;

        std::cout << "Total tests:      " << summary.totalTests << std::endl;
        std::cout << "Passed:           " << summary.passedTests << " ✅" << std::endl;
        std::cout << "Failed:           " << summary.failedTests;
        if (summary.failedTests > 0) std::cout << " ❌";
        std::cout << std::endl;

        std::cout << "Success rate:     " << std::fixed << std::setprecision(1) << (summary.
            GetSuccessRate() * 100.0) << "%" << std::endl;

        auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            summary.totalExecutionTime);
        std::cout << "Total time:       " << totalMs.count() << " ms" << std::endl;

        // Print failed tests details
        if (summary.failedTests > 0)
        {
            std::cout << std::endl << "FAILED TESTS:" << std::endl;
            std::cout << std::string(40, '-') << std::endl;

            for (const auto& result : summary.results)
            {
                if (!result.passed)
                {
                    std::cout << "❌ " << result.testName;
                    if (!result.errorMessage.empty())
                    {
                        std::cout << std::endl << " Error: " << result.errorMessage;
                    }
                    std::cout << std::endl;
                }
            }
        }

        std::cout << std::endl;

        if (summary.AllTestsPassed())
        {
            std::cout << "🎉 ALL TESTS PASSED! 🎉" << std::endl;
        }
        else
        {
            std::cout << "❌ SOME TESTS FAILED ❌" << std::endl;
        }

        std::cout << std::string(60, '=') << std::endl;
    }

    void TestRunner::PrintHeader(size_t testCount)
    {
        std::cout << std::string(60, '=') << std::endl;
        std::cout << "VOID ARCHITECT TEST RUNNER" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        std::cout << "Running " << testCount << " test(s)..." << std::endl;
        std::cout << std::string(60, '-') << std::endl;
    }
}
