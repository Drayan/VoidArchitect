//
// Created by Michael Desmedt on 12/05/2025.
//
#include "Application.hpp"

#include "Logger.hpp"
#include "Systems/Jobs/JobSystem.hpp"
#include "Systems/ConfigSystem.hpp"
#include "Systems/Events/Event.hpp"
#include "Systems/Events/EventSystem.hpp"

#include <SDL3/SDL_timer.h>

namespace VoidArchitect
{
    void Application::Initialize()
    {
        VA_ENGINE_INFO("[Application] Initializing base application systems ...");

        try
        {
            // Initialize the job system
            VA_ENGINE_TRACE("[Application] Initializing job system ...");
            Jobs::g_JobSystem = std::make_unique<Jobs::JobSystem>();

            Events::g_EventSystem = std::make_unique<Events::EventSystem>();

            VA_ENGINE_TRACE("[Application] Initializing config system ...");
            g_ConfigSystem = std::make_unique<ConfigSystem>();

            InitializeSubsystems();
            OnInitialized();
        }
        catch (std::exception& e)
        {
            VA_ENGINE_CRITICAL(
                "[Application] Failed to initialize base application systems: {}",
                e.what());
            throw;
        }
    }

    Application::~Application()
    {
        VA_ENGINE_INFO("[Application] Shutting down base application systems ...");

        g_ConfigSystem = nullptr;
        Events::g_EventSystem = nullptr;
        Jobs::g_JobSystem = nullptr;
    }

    void Application::Run()
    {
        VA_ENGINE_INFO("[Application] Starting main loop ...");

        /// @brief Fixed timestep for consistent simulation (60 FPS)
        constexpr double FIXED_STEP = 1.0 / 60.0;

        /// @brief Time budget for main thread job processing in milliseconds
        ///
        /// This budget prevents main thread jobs from consuming too much frame time.
        /// At 60 FPS (16.67ms per frame), 2ms represents ~12% of the frame budget.
        constexpr float MAIN_THREAD_JOB_BUDGET_MS = 2.0f; // Time budget for main thread jobs
        constexpr float MAIN_THREAD_EVENT_BUDGET_MS = 1.0f; // Time budget for deferred events

        double accumulator = 0.0;
        double currentTime = SDL_GetTicks() / 1000.f;

        // Statistics for monitoring main thread job performance
        uint32_t budgetExceededCount = 0;

        while (m_Running)
        {
            const double newTime = SDL_GetTicks() / 1000.f;
            const double frameTime = newTime - currentTime;
            currentTime = newTime;

            // === Process Main Thread Jobs First ===
            // This ensure main thread jobs (like GPU uploads) are processed consistently
            // even if no WaitFor() calls are made during the frame
            auto jobStats = Jobs::g_JobSystem->ProcessMainThreadJobs(MAIN_THREAD_JOB_BUDGET_MS);
            if (jobStats.budgetExceeded && jobStats.jobsExecuted > 0)
            {
                budgetExceededCount++;

                // Log warning every second (60 frames at 60 fps)
                if (budgetExceededCount % 60 == 0)
                {
                    VA_ENGINE_WARN(
                        "[Application] Main thread job budget exceeded {} times in the last second. "
                        "Executed {} jobs in the last {:.2f} ms.",
                        60,
                        jobStats.jobsExecuted,
                        jobStats.timeSpentMs);
                }
            }

            // Trace detailed statistics every 300 frames (5 seconds at 60 fps)
            static uint32_t frameCount = 0;
            if (++frameCount % 300 == 0 && jobStats.jobsExecuted > 0)
            {
                VA_ENGINE_TRACE(
                    "[Application] Main thread jobs: {} executed in {:.2f}ms "
                    "[Critical: {}, High: {}, Normal: {}, Low: {}]",
                    jobStats.jobsExecuted,
                    jobStats.timeSpentMs,
                    jobStats.jobsByPriority[0],
                    jobStats.jobsByPriority[1],
                    jobStats.jobsByPriority[2],
                    jobStats.jobsByPriority[3]);
            }

            // === Frame Logic ===
            accumulator += frameTime;

            Jobs::g_JobSystem->BeginFrame();
            Events::g_EventSystem->BeginFrame();
            Events::g_EventSystem->ProcessDeferredEvents(MAIN_THREAD_EVENT_BUDGET_MS);

            while (accumulator >= FIXED_STEP)
            {
                OnFixedUpdate(static_cast<float>(FIXED_STEP));
                accumulator -= FIXED_STEP;
            }

            // Call derived class update hook with variable timestep
            OnUpdate(static_cast<float>(frameTime));

            // Call derived class application-specific update hook
            OnLogic(static_cast<float>(frameTime));
        }

        VA_ENGINE_INFO("[Application] Main execution loop terminated.");
        OnShutdownRequested();
    }
} // namespace VoidArchitect
