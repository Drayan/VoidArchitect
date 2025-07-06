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
#include "Systems/Renderer/RenderSystem.hpp"
#include "Systems/ResourceSystem.hpp"
#include "Systems/Jobs/JobSystem.hpp"

namespace VoidArchitect
{
#define BIND_EVENT_FN(x) [this](auto&& PH1) { return this->x(std::forward<decltype(PH1)>(PH1)); }

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
    }

    Application::~Application()
    {
        Renderer::g_RenderSystem = nullptr;
        g_ResourceSystem = nullptr;

        Jobs::g_JobSystem = nullptr;
    }

    void Application::Run()
    {
        constexpr double FIXED_STEP = 1.0 / 60.0;
        constexpr float MAIN_THREAD_JOB_BUDGET_MS = 2.0f; // Time budget for main thread jobs

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
