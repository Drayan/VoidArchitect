//
// Created by Michael Desmedt on 09/07/2025.
//
#include "ClientApplication.hpp"

#include <SDL3/SDL_keycode.h>
#include <VoidArchitect/Engine/Common/Logger.hpp>
#include <VoidArchitect/Engine/Common/Window.hpp>
#include <VoidArchitect/Engine/Common/Systems/Events/EventSystem.hpp>

#include "Platform/SDLWindow.hpp"
#include "Systems/ResourceSystem.hpp"
#include "Systems/RHISystem.hpp"
#include "Systems/Renderer/RenderSystem.hpp"

namespace VoidArchitect
{
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
            // Setup event subscriptions
            m_WindowCloseSubscription = Events::g_EventSystem->Subscribe<Events::WindowCloseEvent>(
                [this](const Events::WindowCloseEvent& e) { OnWindowClose(e); });
            m_WindowResizeSubscription = Events::g_EventSystem->Subscribe<
                Events::WindowResizedEvent>(
                [this](const Events::WindowResizedEvent& e) { OnWindowResize(e); });
            m_KeyPressedSubscription = Events::g_EventSystem->Subscribe<Events::KeyPressedEvent>(
                [this](const Events::KeyPressedEvent& e) { OnKeyPressed(e); });

            // Window Creation
            m_MainWindow = std::unique_ptr<Window>(
                Platform::SDLWindow::Create(WindowProps("VoidArchitect", 1280, 720)));

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

    void ClientApplication::OnLogic(float deltaTime)
    {
        Renderer::g_RenderSystem->RenderFrame(deltaTime);
    }

    void ClientApplication::OnWindowClose(const Events::WindowCloseEvent& e)
    {
        m_Running = false;
    }

    void ClientApplication::OnWindowResize(const Events::WindowResizedEvent& e)
    {
        VA_ENGINE_TRACE("[Application] Window resized to {0}, {1}.", e.GetWidth(), e.GetHeight());
        Renderer::g_RenderSystem->Resize(e.GetWidth(), e.GetHeight());
    }

    void ClientApplication::OnKeyPressed(const Events::KeyPressedEvent& e)
    {
        switch (e.GetKeyCode())
        {
            case SDLK_ESCAPE:
                m_Running = false;
                break;

            // Debug rendering modes
            case SDLK_0:
                Renderer::g_RenderSystem->SetDebugMode(Renderer::RenderSystemDebugMode::None);
                break;
            case SDLK_1:
                Renderer::g_RenderSystem->SetDebugMode(Renderer::RenderSystemDebugMode::Lighting);
                break;
            case SDLK_2:
                Renderer::g_RenderSystem->SetDebugMode(Renderer::RenderSystemDebugMode::Normals);
                break;

            default:
                break;
        }
    }
} // VoidArchitect
