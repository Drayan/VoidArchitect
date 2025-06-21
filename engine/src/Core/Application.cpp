//
// Created by Michael Desmedt on 12/05/2025.
//
#include "Application.hpp"

#include "Events/ApplicationEvent.hpp"
#include "Events/KeyEvent.hpp"
#include "Logger.hpp"
#include "Platform/RHI/IRenderingHardware.hpp"
#include "Window.hpp"

// TEMP Remove this include when we have proper keycode.
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_timer.h>

#include "Platform/Threading/Thread.hpp"
#include "Platform/Threading/ThreadFactory.hpp"
#include "Systems/Renderer/RenderSystem.hpp"
#include "Systems/ResourceSystem.hpp"
#include "Systems/Jobs/JobSystem.hpp"

namespace VoidArchitect
{
#define BIND_EVENT_FN(x) [this](auto&& PH1) { return this->x(std::forward<decltype(PH1)>(PH1)); }

    void TestThreadMain()
    {
        while (!Platform::Thread::ShouldCurrentThreadStop())
        {
            VA_ENGINE_TRACE("[TestThread] IsRunning.");
            Platform::Thread::Sleep(100);
        }
    }

    void ExampleSimpleJobs()
    {
        VA_ENGINE_INFO("=== Example 1 : Simple Jobs ===");

        // Submit a simple job
        auto job1 = Jobs::g_JobSystem->SubmitJob(
            []() -> Jobs::JobResult
            {
                VA_ENGINE_INFO("[Job1] Hello World!");
                Platform::Thread::Sleep(1000);
                return Jobs::JobResult::Success();
            },
            "SimpleJob1");

        // Submit a high priority job
        auto job2 = Jobs::g_JobSystem->SubmitJob(
            []() -> Jobs::JobResult
            {
                VA_ENGINE_INFO("[Job2] Hello World!");
                Platform::Thread::Sleep(100);
                return Jobs::JobResult::Success();
            },
            "HighPriority",
            Jobs::JobPriority::High);

        // Wait for completion
        Jobs::g_JobSystem->WaitForJob(job1);
        Jobs::g_JobSystem->WaitForJob(job2);

        VA_ENGINE_INFO("Simple jobs completed.");
    }

    void ExamplePerformanceMonitoring()
    {
        VA_ENGINE_INFO("=== Example 6: Performance Monitoring ===");

        // Get initial stats
        auto& stats = Jobs::g_JobSystem->GetStats();
        uint64_t initialSubmitted = stats.jobsSubmitted.load();

        // Submit a bunch of jobs for performance testing
        VAArray<Jobs::JobHandle> jobs;
        for (int i = 0; i < 100; ++i)
        {
            auto job = Jobs::g_JobSystem->SubmitJob(
                [i]() -> Jobs::JobResult
                {
                    // Simulate small amount of work
                    volatile int sum = 0;
                    for (int j = 0; j < 1000; ++j)
                    {
                        sum += j;
                    }
                    return Jobs::JobResult::Success();
                },
                "PerfTestJob",
                Jobs::JobPriority::Normal);

            if (job.IsValid())
            {
                jobs.push_back(job);
            }
        }

        // Wait for all jobs to complete
        for (auto& job : jobs)
        {
            Jobs::g_JobSystem->WaitForJob(job);
        }

        // Get final stats
        //stats = Jobs::g_JobSystem->GetStats();
        uint64_t finalSubmitted = stats.jobsSubmitted.load();
        uint64_t completed = stats.jobsCompleted.load();

        VA_ENGINE_INFO("Performance test results:");
        VA_ENGINE_INFO("  Jobs submitted: {}", finalSubmitted - initialSubmitted);
        VA_ENGINE_INFO("  Jobs completed: {}", completed);
        VA_ENGINE_INFO(
            "  Backpressure level: {:.2f}%",
            Jobs::g_JobSystem->GetBackpressureLevel() * 100.0f);

        auto queueLengths = Jobs::g_JobSystem->GetQueueLengths();
        VA_ENGINE_INFO(
            "  Queue lengths: Critical={}, High={}, Normal={}, Low={}",
            queueLengths[0],
            queueLengths[1],
            queueLengths[2],
            queueLengths[3]);
    }

    // Example 4: Backend API with SyncPoints
    void ExampleBackendAPI()
    {
        VA_ENGINE_INFO("=== Example 4: Backend API with SyncPoints ===");

        // Create sync points for complex dependency management
        auto prepSP = Jobs::g_JobSystem->CreateSyncPoint(2, "PreparationComplete");
        auto renderSP = Jobs::g_JobSystem->CreateSyncPoint(1, "RenderComplete");

        // Submit preparation jobs
        Jobs::g_JobSystem->Submit(
            []() -> Jobs::JobResult
            {
                VA_ENGINE_INFO("Preparing textures...");
                Platform::Thread::Sleep(120);
                return Jobs::JobResult::Success();
            },
            prepSP,
            Jobs::JobPriority::Normal,
            "PrepareTextures");

        Jobs::g_JobSystem->Submit(
            []() -> Jobs::JobResult
            {
                VA_ENGINE_INFO("Preparing meshes...");
                Platform::Thread::Sleep(100);
                return Jobs::JobResult::Success();
            },
            prepSP,
            Jobs::JobPriority::Normal,
            "PrepareMeshes");

        // Submit render job that depends on both preparations
        Jobs::g_JobSystem->SubmitAfter(
            prepSP,
            []() -> Jobs::JobResult
            {
                VA_ENGINE_INFO("Rendering scene...");
                Platform::Thread::Sleep(200);
                return Jobs::JobResult::Success();
            },
            renderSP,
            Jobs::JobPriority::Critical,
            "RenderScene");

        // Wait for rendering to complete
        Jobs::g_JobSystem->WaitFor(renderSP);

        VA_ENGINE_INFO("Backend API example completed");
    }

    Application::Application()
    {
        m_MainWindow = std::unique_ptr<Window>(Window::Create());
        m_MainWindow->SetEventCallback(BIND_EVENT_FN(OnEvent));

        // Setting up Subsystems
        try
        {
            Jobs::g_JobSystem = std::make_unique<Jobs::JobSystem>();

            g_ResourceSystem = std::make_unique<ResourceSystem>();
            Renderer::g_RenderSystem = std::make_unique<Renderer::RenderSystem>(
                Platform::RHI_API_TYPE::Vulkan,
                m_MainWindow);

            Renderer::g_RenderSystem->InitializeSubsystems();
        }
        catch (std::exception& e)
        {
            VA_ENGINE_CRITICAL("[Application] Failed to initialize subsystem: {0}", e.what());
            exit(-1);
        }

        m_TestThread = Platform::ThreadFactory::CreateThread();
        m_TestThread->Start(TestThreadMain, "TestThread");

        // ExampleSimpleJobs();
        ExamplePerformanceMonitoring();
        ExampleBackendAPI();
    }

    Application::~Application()
    {
        m_TestThread->RequestStop();
        m_TestThread->Join();

        m_TestThread = nullptr;
        Renderer::g_RenderSystem = nullptr;
        g_ResourceSystem = nullptr;

        Jobs::g_JobSystem = nullptr;
    }

    void Application::Run()
    {
        constexpr double FIXED_STEP = 1.0 / 60.0;
        double accumulator = 0.0;
        double currentTime = SDL_GetTicks() / 1000.f;
        while (m_Running)
        {
            const double newTime = SDL_GetTicks() / 1000.f;
            const double frameTime = newTime - currentTime;
            currentTime = newTime;

            accumulator += frameTime;

            while (accumulator >= FIXED_STEP)
            {
                for (Layer* layer : m_LayerStack) layer->OnFixedUpdate(FIXED_STEP);

                accumulator -= FIXED_STEP;
            }

            Renderer::g_RenderSystem->RenderFrame(frameTime);

            m_MainWindow->OnUpdate();
        }
    }

    void Application::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(OnWindowClose));
        dispatcher.Dispatch<WindowResizedEvent>(BIND_EVENT_FN(OnWindowResized));
        // TEMP This should not stay here; it's just a convenience to hit ESC to quit the app for
        //  now.
        dispatcher.Dispatch<KeyPressedEvent>(BIND_EVENT_FN(OnKeyPressed));

        // Going through the layers backwards to propagate the event.
        for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();)
        {
            (*--it)->OnEvent(e);
            if (e.Handled) break;
        }
    }

    void Application::PushLayer(Layer* layer)
    {
        m_LayerStack.PushLayer(layer);
        layer->OnAttach();
    }

    void Application::PushOverlay(Layer* layer)
    {
        m_LayerStack.PushOverlay(layer);
        layer->OnAttach();
    }

    bool Application::OnWindowClose(WindowCloseEvent& e)
    {
        m_Running = false;
        return true;
    }

    bool Application::OnWindowResized(WindowResizedEvent& e)
    {
        VA_ENGINE_TRACE("[Application] Window resized to {0}, {1}.", e.GetWidth(), e.GetHeight());
        Renderer::g_RenderSystem->Resize(e.GetWidth(), e.GetHeight());
        return true;
    }

    bool Application::OnKeyPressed(KeyPressedEvent& e)
    {
        switch (e.GetKeyCode())
        {
            case SDLK_ESCAPE:
                m_Running = false;
                return true;

            // Debug rendering modes
            case SDLK_0:
                Renderer::g_RenderSystem->SetDebugMode(Renderer::RenderSystemDebugMode::None);
                return true;
            case SDLK_1:
                Renderer::g_RenderSystem->SetDebugMode(Renderer::RenderSystemDebugMode::Lighting);
                return true;
            case SDLK_2:
                Renderer::g_RenderSystem->SetDebugMode(Renderer::RenderSystemDebugMode::Normals);
                return true;

            default:
                break;
        }

        return false;
    }
} // namespace VoidArchitect
