//
// Created by Michael Desmedt on 09/07/2025.
//
#include "ClientApplication.hpp"

#include <SDL3/SDL_keycode.h>
#include <VoidArchitect/Engine/Common/Logger.hpp>
#include <VoidArchitect/Engine/Common/Window.hpp>
#include <VoidArchitect/Engine/Common/Events/Event.hpp>
#include <VoidArchitect/Engine/Common/Events/ApplicationEvent.hpp>
#include <VoidArchitect/Engine/Common/Events/KeyEvent.hpp>

#include "Platform/SDLWindow.hpp"
#include "Systems/ResourceSystem.hpp"
#include "Systems/RHISystem.hpp"
#include "Systems/Renderer/RenderSystem.hpp"

namespace VoidArchitect
{
#define BIND_EVENT_FN(x) [this](auto&& PH1) { return this->x(std::forward<decltype(PH1)>(PH1)); }

    ClientApplication::ClientApplication()
        : Application(),
          m_MainWindow(nullptr)
    {
    }

    void ClientApplication::InitializeSubsystems()
    {
        VA_ENGINE_INFO("[ClientApplication] Initializing client subsystems...");

        try
        {
            // Window Creation
            m_MainWindow = std::unique_ptr<Window>(
                Platform::SDLWindow::Create(WindowProps("VoidArchitect", 1280, 720)));
            m_MainWindow->SetEventCallback(BIND_EVENT_FN(OnEvent));

            // Resource Management System
            g_ResourceSystem = std::make_unique<ResourceSystem>();

            // RHI Selection
            RHI::g_RHISystem = std::make_unique<RHI::RHISystem>();
            auto rhi = RHI::g_RHISystem->CreateBestAvailableRHI(m_MainWindow);
            Renderer::g_RenderSystem = std::make_unique<Renderer::RenderSystem>(rhi, m_MainWindow);
            Renderer::g_RenderSystem->InitializeSubsystems();
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_CRITICAL("[ClientApplication] Failed to initialize subsystems: {}", e.what());
            throw;
        }
    }

    ClientApplication::~ClientApplication()
    {
        if (Renderer::g_RenderSystem) Renderer::g_RenderSystem = nullptr;

        if (RHI::g_RHISystem) RHI::g_RHISystem = nullptr;

        if (g_ResourceSystem) g_ResourceSystem = nullptr;

        if (m_MainWindow) m_MainWindow = nullptr;
    }

    void ClientApplication::OnUpdate(float deltaTime)
    {
        m_MainWindow->OnUpdate();
    }

    void ClientApplication::OnApplicationUpdate(float deltaTime)
    {
        Renderer::g_RenderSystem->RenderFrame(deltaTime);
    }

    void ClientApplication::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);

        dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(OnWindowClose));
        dispatcher.Dispatch<WindowResizedEvent>(BIND_EVENT_FN(OnWindowResize));
        dispatcher.Dispatch<KeyPressedEvent>(BIND_EVENT_FN(OnKeyPressed));

        Application::OnEvent(e);
    }

    bool ClientApplication::OnWindowClose(WindowCloseEvent& e)
    {
        m_Running = false;

        return true;
    }

    bool ClientApplication::OnWindowResize(WindowResizedEvent& e)
    {
        VA_ENGINE_TRACE("[Application] Window resized to {0}, {1}.", e.GetWidth(), e.GetHeight());
        Renderer::g_RenderSystem->Resize(e.GetWidth(), e.GetHeight());
        return true;
    }

    bool ClientApplication::OnKeyPressed(KeyPressedEvent& e)
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
} // VoidArchitect
