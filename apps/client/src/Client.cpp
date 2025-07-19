//
// Created by Michael Desmedt on 12/05/2025.
//
#include <VoidArchitect/Engine/Common/VoidArchitect.hpp>
#include <VoidArchitect/Engine/Client/ClientApplication.hpp>
#include <VoidArchitect/Engine/Client/Systems/Renderer/RenderSystem.hpp>
#include <VoidArchitect/Engine/Client/Systems/Renderer/DebugCameraController.hpp>

class Client final : public VoidArchitect::ClientApplication
{
public:
    Client() = default;

protected:
    void InitializeSubsystems() override
    {
        ClientApplication::InitializeSubsystems();

        auto& camera = VoidArchitect::Renderer::g_RenderSystem->GetMainCamera();
        m_DebugCameraController = std::make_unique<VoidArchitect::Renderer::DebugCameraController>(
            camera);
        camera.SetPosition({0.f, 0.f, 3.f});
    }

    void OnFixedUpdate(float fixedDeltaTime) override
    {
        m_DebugCameraController->OnFixedUpdate(fixedDeltaTime);
    }

private:
    std::unique_ptr<VoidArchitect::Renderer::DebugCameraController> m_DebugCameraController;
};

VoidArchitect::Application* VoidArchitect::CreateApplication() { return new Client(); }
